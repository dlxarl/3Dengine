#include "Platform.h"

#import <Cocoa/Cocoa.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

namespace fs = std::filesystem;

@interface AureonMenuTarget : NSObject
- (void)fire:(NSMenuItem*)item;
@end

namespace {
    std::vector<int> pendingCommands;
    AureonMenuTarget* menuTarget = nil;
    std::string lastMenuKey;
}

@implementation AureonMenuTarget
- (void)fire:(NSMenuItem*)item {
    pendingCommands.push_back((int)[item tag]);
}
@end

namespace {
    NSString* toNSString(const std::string& text) {
        return [NSString stringWithUTF8String:text.c_str()];
    }

    fs::path toPath(NSURL* url) {
        return fs::u8path([[url path] UTF8String]);
    }
}

bool Platform::hasNativeDialogs() {
    return true;
}

fs::path Platform::chooseFolder(const std::string& title, const fs::path& startIn) {
    @autoreleasepool {
        NSOpenPanel* panel = [NSOpenPanel openPanel];
        [panel setMessage:toNSString(title)];
        [panel setCanChooseDirectories:YES];
        [panel setCanChooseFiles:NO];
        [panel setCanCreateDirectories:YES];
        [panel setAllowsMultipleSelection:NO];
        if (!startIn.empty()) {
            [panel setDirectoryURL:[NSURL fileURLWithPath:toNSString(startIn.u8string())]];
        }
        if ([panel runModal] == NSModalResponseOK) {
            return toPath([panel URL]);
        }
        return {};
    }
}

std::vector<fs::path> Platform::chooseFiles(const std::string& title, const std::vector<std::string>& extensions) {
    @autoreleasepool {
        NSOpenPanel* panel = [NSOpenPanel openPanel];
        [panel setMessage:toNSString(title)];
        [panel setCanChooseDirectories:NO];
        [panel setCanChooseFiles:YES];
        [panel setAllowsMultipleSelection:YES];

        NSMutableArray<UTType*>* types = [NSMutableArray array];
        for (const std::string& extension : extensions) {
            if (UTType* type = [UTType typeWithFilenameExtension:toNSString(extension)]) {
                [types addObject:type];
            }
        }
        if ([types count] > 0) {
            [panel setAllowedContentTypes:types];
        }

        std::vector<fs::path> chosen;
        if ([panel runModal] == NSModalResponseOK) {
            for (NSURL* url in [panel URLs]) {
                chosen.push_back(toPath(url));
            }
        }
        return chosen;
    }
}

void Platform::revealInFileManager(const fs::path& path) {
    @autoreleasepool {
        NSURL* url = [NSURL fileURLWithPath:toNSString(path.u8string())];
        [[NSWorkspace sharedWorkspace] activateFileViewerSelectingURLs:@[url]];
    }
}

bool Platform::moveToTrash(const fs::path& path, std::string* error) {
    @autoreleasepool {
        NSError* failure = nil;
        NSURL* url = [NSURL fileURLWithPath:toNSString(path.u8string())];
        BOOL moved = [[NSFileManager defaultManager] trashItemAtURL:url resultingItemURL:nil error:&failure];
        if (!moved && error && failure) {
            *error = [[failure localizedDescription] UTF8String];
        }
        return moved;
    }
}

namespace {
    void describe(const std::vector<Platform::MenuItem>& items, std::string& out) {
        for (const Platform::MenuItem& item : items) {
            out += item.title + '|' + std::to_string(item.command) + '|' + item.key + (item.shift ? 'S' : '-') +
                   (item.enabled ? 'E' : '-') + (item.checked ? 'C' : '-') + (item.separator ? '_' : '-') + '{';
            describe(item.children, out);
            out += '}';
        }
    }

    NSMenuItem* makeItem(const Platform::MenuItem& item);

    NSMenu* makeMenu(NSString* title, const std::vector<Platform::MenuItem>& items) {
        NSMenu* menu = [[NSMenu alloc] initWithTitle:title];
        [menu setAutoenablesItems:NO];
        for (const Platform::MenuItem& item : items) [menu addItem:makeItem(item)];
        return menu;
    }

    NSMenuItem* makeItem(const Platform::MenuItem& item) {
        if (item.separator) return [NSMenuItem separatorItem];

        NSMenuItem* entry = [[NSMenuItem alloc] initWithTitle:toNSString(item.title) action:nil keyEquivalent:@""];
        if (!item.children.empty()) {
            [entry setSubmenu:makeMenu(toNSString(item.title), item.children)];
            return entry;
        }
        if (item.command != 0) {
            [entry setTarget:menuTarget];
            [entry setAction:@selector(fire:)];
            [entry setTag:item.command];
            [entry setEnabled:item.enabled];
        } else {
            [entry setEnabled:NO];
        }
        if (!item.key.empty()) {
            [entry setKeyEquivalent:toNSString(item.key)];
            NSEventModifierFlags flags = NSEventModifierFlagCommand;
            if (item.shift) flags |= NSEventModifierFlagShift;
            [entry setKeyEquivalentModifierMask:flags];
        }
        if (item.checked) [entry setState:NSControlStateValueOn];
        return entry;
    }
}

void Platform::setMenuBar(const std::string& appName, const std::vector<MenuItem>& menus) {
    std::string key = appName + '\n';
    describe(menus, key);
    if (key == lastMenuKey) return;
    lastMenuKey = key;

    @autoreleasepool {
        if (!menuTarget) menuTarget = [[AureonMenuTarget alloc] init];
        NSString* name = toNSString(appName);

        NSMenu* bar = [[NSMenu alloc] initWithTitle:@""];

        // The application menu, which macOS expects first and titles with the app's name.
        NSMenuItem* appEntry = [[NSMenuItem alloc] init];
        NSMenu* appMenu = [[NSMenu alloc] initWithTitle:name];
        [appMenu addItemWithTitle:[@"Hide " stringByAppendingString:name] action:@selector(hide:) keyEquivalent:@"h"];
        NSMenuItem* hideOthers = [appMenu addItemWithTitle:@"Hide Others" action:@selector(hideOtherApplications:)
                                             keyEquivalent:@"h"];
        [hideOthers setKeyEquivalentModifierMask:NSEventModifierFlagCommand | NSEventModifierFlagOption];
        [appMenu addItemWithTitle:@"Show All" action:@selector(unhideAllApplications:) keyEquivalent:@""];
        [appMenu addItem:[NSMenuItem separatorItem]];
        [appMenu addItemWithTitle:[@"Quit " stringByAppendingString:name] action:@selector(terminate:) keyEquivalent:@"q"];
        [appEntry setSubmenu:appMenu];
        [bar addItem:appEntry];

        for (const MenuItem& menu : menus) {
            NSMenuItem* entry = [[NSMenuItem alloc] initWithTitle:toNSString(menu.title) action:nil keyEquivalent:@""];
            [entry setSubmenu:makeMenu(toNSString(menu.title), menu.children)];
            [bar addItem:entry];
            if (menu.title == "Window") [NSApp setWindowsMenu:[entry submenu]];
        }

        [NSApp setMainMenu:bar];
    }
}

std::vector<int> Platform::takeMenuCommands() {
    std::vector<int> taken;
    taken.swap(pendingCommands);
    return taken;
}
