# UI Layout Folder

To prioritize stability, cleanliness, and ease of cross-platform compilation without requiring the Qt User Interface Compiler (UIC), the entire user interface, layouts, and widget hierarchies are set up **programmatically** in C++:

- UI Layout Code: [MainWindow.cpp](file:///e:/SDE_intern_Task/LANChat/src/MainWindow.cpp)
- UI Declarations: [MainWindow.h](file:///e:/SDE_intern_Task/LANChat/include/MainWindow.h)

This eliminates dependency on `.ui` XML files and makes the application lightweight and robust.
