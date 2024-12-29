# File System Simulation

This project is a File System Simulation built using the C programming language and GTK (GIMP Toolkit) for the graphical user interface (GUI). The application provides a simulated file system environment with basic file management functionalities, including navigation, file creation, deletion, and other essential file system operations.

## Features

- **Graphical User Interface:**
  - User-friendly GUI built with GTK for managing files and directories.
  - Toolbar with icons for commonly used actions like creating files, folders, and searching.

- **Simulated File System:**
  - Create and delete files and directories.
  - Navigate through directories.
  - Rename files and folders.

- **Search Functionality:**
  - Integrated search bar to find files or directories by name.

## Prerequisites

Before running the application, ensure you have the following installed on your system:

- GCC (GNU Compiler Collection)
- GTK 3 Development Libraries and Tools
  
### Installing GTK 3 Development Tools

#### On Ubuntu/Debian:
```bash
sudo apt update
sudo apt install libgtk-3-dev
```

#### On Fedora:
```bash
sudo dnf install gtk3-devel
```

#### On Windows:
Download and set up [MSYS2](https://www.msys2.org/) to install GTK libraries.

## Building the Project

1. Clone this repository:
   ```bash
   git clone <repository-url>
   cd <repository-directory>
   ```

2. Compile the source code:
   ```bash
   gcc -o filesystem_simulation main.c `pkg-config --cflags --libs gtk+-3.0`
   ```

3. Run the application:
   ```bash
   ./filesystem_simulation
   ```

## File Structure

- `main.c`: The main source file containing the program logic.
- `filesystem.h`: Header file defining file system operations.
- `gui.c`: Source file handling GTK-based GUI functionality.
- `Makefile`: Automates the build process (optional).

## Usage

1. Launch the application.
2. Use the toolbar to perform the following actions:
   - **Create File:** Add a new file to the current directory.
   - **Create Folder:** Add a new folder to the current directory.
   - **Delete:** Remove selected files or folders.
   - **Search:** Locate files or directories using the search bar.
3. Navigate through directories using the interface.

## Screenshots

![Application Screenshot](screenshot.png)

## Known Issues

- The simulation does not persist changes after the application is closed.
- Limited support for file attributes and metadata.

## Future Enhancements

- Add support for drag-and-drop operations.
- Implement file properties dialog.
- Enhance search functionality with filters and regular expressions.
- Introduce persistence for the simulated file system using serialization.

## Contributing

Contributions are welcome! To contribute:

1. Fork the repository.
2. Create a feature branch (`git checkout -b feature-name`).
3. Commit your changes (`git commit -m 'Add some feature'`).
4. Push to the branch (`git push origin feature-name`).
5. Open a Pull Request.

## License

This project is licensed under the MIT License. See the `LICENSE` file for details.

## Acknowledgments

- GTK documentation and community for extensive resources.
- [Icons8](https://icons8.com/) for the toolbar icons.

