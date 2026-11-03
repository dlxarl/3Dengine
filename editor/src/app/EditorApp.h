#pragma once
#include "EditorContext.h"
#include "Project.h"

// One editing session of one project, in its own window. Returns how the session ended.
class EditorApp {
public:
    explicit EditorApp(Project project);

    EditorExit run();

private:
    Project project;
};
