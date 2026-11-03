#include "Platform.h"

#import <Cocoa/Cocoa.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

namespace fs = std::filesystem;

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
