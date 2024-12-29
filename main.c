#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "filesystem.h" // Include your filesystem header

static GtkWidget *window;
static GtkTreeView *tree_view;
static GtkListStore *store;
static DirectoryStruct *current_directory;
static DirectoryStruct *root_directory;

static GdkPixbuf *folder_pixbuf = NULL;
static GdkPixbuf *file_pixbuf = NULL;

#define ICON_SIZE 32




// Function to show an error dialog
static void show_error_dialog(const gchar *message) {
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(window),
                                               GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_ERROR,
                                               GTK_BUTTONS_CLOSE,
                                               "%s", message);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}





// ... (keep existing function declarations)

/// New function to refresh the file list
static void refresh_file_list() {
        // Set the desired icon size
    int icon_width = 32;
    int icon_height = 32;

    gtk_list_store_clear(store);
    GdkPixbuf *folder_pixbuf = gdk_pixbuf_new_from_file("folder.png", NULL);
    GdkPixbuf *file_pixbuf = gdk_pixbuf_new_from_file("file.png", NULL);


    if (!folder_pixbuf || !file_pixbuf) {
        show_error_dialog("Failed to load icons.");
        if (folder_pixbuf) g_object_unref(folder_pixbuf);
        if (file_pixbuf) g_object_unref(file_pixbuf);
        return;
    }

    GdkPixbuf *scaled_folder_pixbuf = gdk_pixbuf_scale_simple(folder_pixbuf, icon_width, icon_height, GDK_INTERP_BILINEAR);
    GdkPixbuf *scaled_file_pixbuf = gdk_pixbuf_scale_simple(file_pixbuf, icon_width, icon_height, GDK_INTERP_BILINEAR);

    // Populate the list with files and directories in the current directory
    for (int i = 0; i < current_directory->child_count; i++) {
        GtkTreeIter iter;
        gtk_list_store_append(store, &iter);

        const char* item_type = current_directory->children[i]->is_directory ? "Folder" : "File";
        GdkPixbuf *icon = current_directory->children[i]->is_directory ? scaled_folder_pixbuf : scaled_file_pixbuf;


        gtk_list_store_set(store, &iter,
                           0, icon,
                           1, current_directory->children[i]->name,
                           2, item_type,
                           -1);
    }
    g_object_unref(folder_pixbuf);
    g_object_unref(file_pixbuf);

}


/// NAVIGATE FUNCTION
static void on_back_clicked(GtkWidget *widget, gpointer data) {
    if (current_directory->parent != NULL) {
        current_directory = current_directory->parent;
        refresh_file_list();
    }
}


/// ON FILE CLICKED
static void on_file_clicked(GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data) {
    GtkTreeIter iter;
    gchar *icon, *name, *type;
    GtkTreeModel *model = gtk_tree_view_get_model(tree_view);

    if (gtk_tree_model_get_iter(model, &iter, path)) {
        gtk_tree_model_get(model, &iter, 0,&icon, 1, &name, 2, &type, -1);

        if (strcmp(type, "Folder") == 0) {
            DirectoryStruct *new_dir = find_directory(current_directory, name);
            if (new_dir) {
                current_directory = new_dir;
                refresh_file_list();
            } else {
                show_error_dialog("Failed to change directory.");
            }
        } else {
            int file_descriptor = open_file(name);
            if (file_descriptor != -1) {
                char buffer[50];
                int bytes_read = read_file(file_descriptor, buffer, BUFFER_SIZE);
                buffer[bytes_read] = '\0';
                char str[20];
                sprintf(str, "%d", bytes_read);
                if (bytes_read > 0) {
                    // Display file contents (you might want to create a new window for this)
                    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(window),
                                                               GTK_DIALOG_DESTROY_WITH_PARENT,
                                                               GTK_MESSAGE_INFO,
                                                               GTK_BUTTONS_OK,
                                                               "File contents:\n%.*s", "testing file", buffer);
                    gtk_dialog_run(GTK_DIALOG(dialog));
                    gtk_widget_destroy(dialog);
                }
                close_file(file_descriptor);
            } else {
                show_error_dialog("Failed to open file.");
            }
        }

        g_free(name);
        g_free(type);
    }
}


/// ON CREATE FOLDER
static void on_create_folder(GtkWidget *widget, gpointer data) {
    GtkWidget *dialog;
    GtkWidget *content_area;
    GtkWidget *entry;
    GtkDialogFlags flags;

    flags = GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT;
    dialog = gtk_dialog_new_with_buttons("Create New Folder",
                                         GTK_WINDOW(window),
                                         flags,
                                         "Cancel",
                                         GTK_RESPONSE_CANCEL,
                                         "Create",
                                         GTK_RESPONSE_ACCEPT,
                                         NULL);

    content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Enter folder name");
    gtk_container_add(GTK_CONTAINER(content_area), entry);
    gtk_widget_show_all(dialog);

    gint result = gtk_dialog_run(GTK_DIALOG(dialog));
    if (result == GTK_RESPONSE_ACCEPT) {
        const gchar *folder_name = gtk_entry_get_text(GTK_ENTRY(entry));
        if (strlen(folder_name) > 0) {
            DirectoryStruct *new_dir = create_dir(folder_name, current_directory);
            if (new_dir) {
                refresh_file_list();
            } else {
                show_error_dialog("Failed to create folder.");
            }
        } else {
            show_error_dialog("Folder name cannot be empty.");
        }
    }

    gtk_widget_destroy(dialog);
}


/// ON DELETE
static void on_delete(GtkWidget *widget, gpointer data) {
    GtkTreeSelection *selection;
    GtkTreeModel *model;
    GtkTreeIter iter;

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_view));
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gchar *name = NULL;
        gchar *type = NULL;

        gtk_tree_model_get(model, &iter, 1, &name, 2, &type, -1);

        GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(window),
                                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                                   GTK_MESSAGE_QUESTION,
                                                   GTK_BUTTONS_YES_NO,
                                                   "Are you sure you want to delete '%s'?", name);
        gint result = gtk_dialog_run(GTK_DIALOG(dialog));

        if (result == GTK_RESPONSE_YES) {
            if (strcmp(type, "File") == 0) {
                delete_file(name);
                g_print("deleted successfuly");
                refresh_file_list();

            } else {
                DirectoryStruct *dir_to_delete = find_directory(current_directory, name);
                if (dir_to_delete) {
                    delete_directory(dir_to_delete);
                    g_print("Directory %s deleted successfully.\n", name);
                } else {
                    show_error_dialog("Error: Directory not found.");
                }

                refresh_file_list();

                // The delete_file function already prints success or error messages
            }
            refresh_file_list();
        }

        gtk_widget_destroy(dialog);
        g_free(name);
        g_free(type);
    } else {
        show_error_dialog("Please select an item to delete.");
    }
}


/// GUI CREATE FILE
static void on_create_file(GtkWidget *widget, gpointer data) {
    GtkWidget *dialog;
    GtkWidget *content_area;
    GtkWidget *entry;
    GtkWidget *read_checkbox, *write_checkbox, *execute_checkbox;
    GtkDialogFlags flags;

    flags = GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT;
    dialog = gtk_dialog_new_with_buttons("New File",
                                         GTK_WINDOW(window),
                                         flags,
                                         "Cancel",
                                         GTK_RESPONSE_CANCEL,
                                         "Create",
                                         GTK_RESPONSE_ACCEPT,
                                         NULL);

    content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Enter file name");
    gtk_container_add(GTK_CONTAINER(content_area), entry);

    // Permissions checkboxes
    read_checkbox = gtk_check_button_new_with_label("Read");
    write_checkbox = gtk_check_button_new_with_label("Write");
    execute_checkbox = gtk_check_button_new_with_label("Execute");

    gtk_container_add(GTK_CONTAINER(content_area), read_checkbox);
    gtk_container_add(GTK_CONTAINER(content_area), write_checkbox);
    gtk_container_add(GTK_CONTAINER(content_area), execute_checkbox);
    gtk_widget_show_all(dialog);

    gint result = gtk_dialog_run(GTK_DIALOG(dialog));
    if (result == GTK_RESPONSE_ACCEPT) {
        const gchar *file_name = gtk_entry_get_text(GTK_ENTRY(entry));
                // Get permissions from checkboxes
        int permissions = 0;
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(read_checkbox)))  permissions |= 4;
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(write_checkbox))) permissions |= 2;
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(execute_checkbox))) permissions |= 1;

        if (strlen(file_name) > 0) {
            // Assuming create_file function exists in your filesystem.h
            int inode_number = create_file(file_name, 100, permissions);  // Creating an empty file
            if (inode_number != -1) {
                // Add the new file to the current directory
                DirectoryStruct *new_file = malloc(sizeof(DirectoryStruct));
                strcpy(new_file->name, file_name);
                new_file->parent = current_directory;
                new_file->children = NULL;
                new_file->child_count = 0;
                new_file->max_children = 0;
                new_file->is_directory = 0;
                new_file->inode_number = inode_number;
                set_permissions(inode_number, permissions);

                // Add to current directory's children
                if (current_directory->child_count >= current_directory->max_children) {
                    current_directory->max_children = current_directory->max_children ? current_directory->max_children * 2 : 1;
                    current_directory->children = realloc(current_directory->children, current_directory->max_children * sizeof(DirectoryStruct*));
                }
                current_directory->children[current_directory->child_count++] = new_file;

                refresh_file_list();
            } else {
                show_error_dialog("Failed to create file.");
            }
        } else {
            show_error_dialog("File name cannot be empty.");
        }
    }

    gtk_widget_destroy(dialog);
}


/// WRITE TO FILE


int write_file_by_path(DirectoryStruct *root, const char *path, const char *text) {
    DirectoryStruct *file_node = navigate_path(root, path);
    if (file_node == NULL || file_node->is_directory) {
        return -1; // Error: invalid file or directory
    }

    int inode_number = file_node->inode_number;

    // Check write permissions
    if (!check_permissions(inode_number, 2)) { // 2 is write permission
        return -2; // Error: no write permission
    }

    int fd = open_file(file_node->name);
    if (fd == -1) {
        return -3; // Error: failed to open file
    }

    // Use your write_file function
    int bytes_written = write_file(fd, text, strlen(text));

    close_file(fd);

    return bytes_written;
}


void write_to_file(GtkWidget *widget, gpointer data, DirectoryStruct *current_directory) {
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_view));
    GtkTreeModel *model;
    GtkTreeIter iter;

    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        // 1. Get Selected Item Info:
        gchar *name, *icon, *type; // Assuming columns are: 0: Icon, 1: Name, 2: Type
        gtk_tree_model_get(model, &iter, 0, &icon, 1, &name, 2, &type, -1);

        // 2. Check if Folder or File:
        if (strcmp(type, "Folder") == 0) { // Or whatever string you use to denote folders
            // Handle folder selection (e.g., change directory)
            show_error_dialog("Cannot write to a directory.");
        } else {
            // 3. Dialog Setup (for file):
            GtkWidget *dialog;
            GtkWidget *content_area;
            GtkWidget *entry;
            GtkDialogFlags flags;

            flags = GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT;
            dialog = gtk_dialog_new_with_buttons("Write to file",
                                                 GTK_WINDOW(window),
                                                 flags,
                                                 "Cancel",
                                                 GTK_RESPONSE_CANCEL,
                                                 "Write",
                                                 GTK_RESPONSE_ACCEPT,
                                                 NULL);

            content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
            entry = gtk_entry_new();
            gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Enter Content");
            gtk_container_add(GTK_CONTAINER(content_area), entry);
            gtk_widget_show_all(dialog);
            // 5. Handle User Response:
            gint result = gtk_dialog_run(GTK_DIALOG(dialog));
            if (result == GTK_RESPONSE_ACCEPT) {
            const char *text_to_write = gtk_entry_get_text(GTK_ENTRY(entry));

            if (strlen(text_to_write) == 0) {
                show_error_dialog("Text to write cannot be empty.");
            } else {
                int fd = open_file(name);
                if (fd != -1) {
                    int bytes_written = write_file(fd, text_to_write, strlen(text_to_write));
                    close_file(fd);
                    if (bytes_written >= 0) {
                        refresh_file_list();
                        show_error_dialog("File written successfully.");
                    } else {
                        show_error_dialog("You do not have permission to write to this file.");
                    }
                } else {
                    show_error_dialog("Failed to open file for writing.");
                }
            }
        }
                gtk_widget_destroy(dialog);
        }

        g_free(icon); // Free memory allocated by gtk_tree_model_get
        g_free(name);
        g_free(type);
    } else {
        show_error_dialog("No item selected.");
    }
}


/// RENAME
static void on_rename(GtkWidget *widget, gpointer data) {
    GtkTreeSelection *selection;
    GtkTreeModel *model;
    GtkTreeIter iter;


    // Get the selection from the tree view
    selection = gtk_tree_view_get_selection(tree_view);
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gchar *name, *icon, *type;
        gboolean is_directory;

        // Retrieve the current name and type (file or directory) of the selected item
        gtk_tree_model_get(model, &iter, 0, &icon, 1, &name, 2, &type, -1);

        GtkWidget *dialog;
        GtkWidget *content_area;
        GtkWidget *entry;
        GtkDialogFlags flags;

        // Create a modal dialog for renaming
        flags = GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT;
        dialog = gtk_dialog_new_with_buttons("Rename",
                                             GTK_WINDOW(window),
                                             flags,
                                             "Cancel",
                                             GTK_RESPONSE_CANCEL,
                                             "Rename",
                                             GTK_RESPONSE_ACCEPT,
                                             NULL);

        // Add an entry field to the dialog
        content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
        entry = gtk_entry_new();
        gtk_entry_set_text(GTK_ENTRY(entry), name);
        gtk_container_add(GTK_CONTAINER(content_area), entry);
        gtk_widget_show_all(dialog);

        // Run the dialog and get the response
        gint result = gtk_dialog_run(GTK_DIALOG(dialog));
         if (result == GTK_RESPONSE_ACCEPT) {
        const gchar *new_name = gtk_entry_get_text(GTK_ENTRY(entry));

        if (strlen(new_name) > 0 && strcmp(name, new_name) != 0) {
            if (strcmp(type, "Folder") == 0){
                if (rename_directory(root_directory, name, new_name) == 0) {
                    refresh_file_list();
                } else {
                    show_error_dialog("Failed to rename directory.");
                }
            } else {
                if (rename_file(name, new_name) == 0) {
                    refresh_file_list();
                } else {
                    show_error_dialog("Failed to rename file.");
                }
            }
        }
    }

        // Destroy the dialog and free memory
        gtk_widget_destroy(dialog);
        g_free(name);
        g_free(icon);
        g_free(type);
    } else {
        // Show an error dialog if no item is selected
        show_error_dialog("Please select an item to rename.");
    }
}

static GdkPixbuf* resize_pixbuf(GdkPixbuf *original, int width, int height) {
    return gdk_pixbuf_scale_simple(original, width, height, GDK_INTERP_BILINEAR);
}


/// ON SEARCH
static void search_directory(DirectoryStruct *dir, const char *search_term) {
    for (int i = 0; i < dir->child_count; i++) {
        if (strstr(dir->children[i]->name, search_term) != NULL) {
            GtkTreeIter iter;
            gtk_list_store_append(store, &iter);

            const char* item_type = dir->children[i]->is_directory ? "Folder" : "File";
            GdkPixbuf *icon = dir->children[i]->is_directory ? folder_pixbuf : file_pixbuf;

            if (icon) {
                gtk_list_store_set(store, &iter,
                                   0, icon,
                                   1, dir->children[i]->name,
                                   2, item_type,
                                   -1);
            } else {
                // If icon is NULL, skip setting the icon
                gtk_list_store_set(store, &iter,
                                   1, dir->children[i]->name,
                                   2, item_type,
                                   -1);
            }
        }

        if (dir->children[i]->is_directory) {
            search_directory(dir->children[i], search_term);
        }
    }
}

/// SEARCH
static void on_search(GtkWidget *widget, gpointer data) {
    GtkEntry *entry = GTK_ENTRY(data);
    const gchar *search_term = gtk_entry_get_text(entry);

    if (strlen(search_term) == 0) {
        show_error_dialog("Please enter a search term.");
        return;
    }

    gtk_list_store_clear(store);

    search_directory(current_directory, search_term);

    // If no results were found, show a message
    if (gtk_tree_model_iter_n_children(GTK_TREE_MODEL(store), NULL) == 0) {
        show_error_dialog("No matching files or folders found.");
    }
}


/// ACTIVATE THE FILE SYSTEM
static void activate(GtkApplication *app, gpointer user_data) {
    GtkWidget *grid;
    GtkWidget *scrolled_window;
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;
    GtkWidget *toolbar;
    GtkWidget *search_entry;
    GtkToolItem *search_item, *search_button;
    GtkToolItem *new_folder_button, *delete_button, *rename_button, *new_file_button, *write_file_button;

    // Create main window
    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "File System GUI");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);

    // Create grid layout
    grid = gtk_grid_new();
    gtk_container_add(GTK_CONTAINER(window), grid);

    // Create toolbar
    toolbar = gtk_toolbar_new();
    gtk_grid_attach(GTK_GRID(grid), toolbar, 0, 0, 1, 1);

    new_folder_button = gtk_tool_button_new(NULL, "New Folder");
    GdkPixbuf *new_folder_pixbuf = gdk_pixbuf_new_from_file("new-folder.png", NULL);
    if (new_folder_pixbuf) {
        new_folder_pixbuf = gdk_pixbuf_scale_simple(new_folder_pixbuf, ICON_SIZE, ICON_SIZE, GDK_INTERP_BILINEAR);
        gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(new_folder_button), gtk_image_new_from_pixbuf(new_folder_pixbuf));
        g_object_unref(new_folder_pixbuf); // Free the pixbuf
    } else {
        fprintf(stderr, "Failed to load icon: new_folder.png\n");
    }
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), new_folder_button, -1);
    g_signal_connect(new_folder_button, "clicked", G_CALLBACK(on_create_folder), NULL);


    GtkToolItem *separator = gtk_separator_tool_item_new();
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), separator, -1);




    // New File Button
    new_file_button = gtk_tool_button_new(NULL, "New File");
    GdkPixbuf *new_file_pixbuf = gdk_pixbuf_new_from_file("add-file.png", NULL);
    if (new_file_pixbuf) {
        new_file_pixbuf = gdk_pixbuf_scale_simple(new_file_pixbuf, ICON_SIZE, ICON_SIZE, GDK_INTERP_BILINEAR);
        gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(new_file_button), gtk_image_new_from_pixbuf(new_file_pixbuf));
        g_object_unref(new_file_pixbuf); // Free the pixbuf
    } else {
        fprintf(stderr, "Failed to load icon: add-file.png\n");
    }
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), new_file_button, -1);
    g_signal_connect(new_file_button, "clicked", G_CALLBACK(on_create_file), NULL);

    // Delete Button
    delete_button = gtk_tool_button_new(NULL, "Delete");
    GdkPixbuf *delete_pixbuf = gdk_pixbuf_new_from_file("delete.png", NULL);
    if (delete_pixbuf) {
        delete_pixbuf = gdk_pixbuf_scale_simple(delete_pixbuf, ICON_SIZE, ICON_SIZE, GDK_INTERP_BILINEAR);
        gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(delete_button), gtk_image_new_from_pixbuf(delete_pixbuf));
        g_object_unref(delete_pixbuf);
    } else {
        fprintf(stderr, "Failed to load icon: delete.png\n");
    }
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), delete_button, -1);
    g_signal_connect(delete_button, "clicked", G_CALLBACK(on_delete), NULL);

    // Rename Button
    rename_button = gtk_tool_button_new(NULL, "Rename");
    GdkPixbuf *rename_pixbuf = gdk_pixbuf_new_from_file("rename.png", NULL);
    if (rename_pixbuf) {
        rename_pixbuf = gdk_pixbuf_scale_simple(rename_pixbuf, ICON_SIZE, ICON_SIZE, GDK_INTERP_BILINEAR);
        gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(rename_button), gtk_image_new_from_pixbuf(rename_pixbuf));
        g_object_unref(rename_pixbuf);
    } else {
        fprintf(stderr, "Failed to load icon: rename.png\n");
    }
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), rename_button, -1);
    g_signal_connect(rename_button, "clicked", G_CALLBACK(on_rename), NULL);



    // write
    write_file_button = gtk_tool_button_new(NULL, "Write");
    GdkPixbuf *write_file_pixbuf = gdk_pixbuf_new_from_file("write.png", NULL);
    if (write_file_pixbuf) {
        write_file_pixbuf = gdk_pixbuf_scale_simple(write_file_pixbuf, ICON_SIZE, ICON_SIZE, GDK_INTERP_BILINEAR);
        gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(write_file_button), gtk_image_new_from_pixbuf(write_file_pixbuf));
        g_object_unref(write_file_pixbuf);
    } else {
        fprintf(stderr, "Failed to load icon: rename.png\n");
    }
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), write_file_button, -1);
    g_signal_connect(write_file_button, "clicked", G_CALLBACK(write_to_file), current_directory);

    // Back Button
    GtkToolItem *back_button = gtk_tool_button_new(NULL, "Back");
    GdkPixbuf *back_pixbuf = gdk_pixbuf_new_from_file("previous.png", NULL);
    if (back_pixbuf) {
        back_pixbuf = gdk_pixbuf_scale_simple(back_pixbuf, ICON_SIZE, ICON_SIZE, GDK_INTERP_BILINEAR);
        gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(back_button), gtk_image_new_from_pixbuf(back_pixbuf));
        g_object_unref(back_pixbuf);
    } else {
        fprintf(stderr, "Failed to load icon: back.png\n");
    }
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), back_button, 0);
    g_signal_connect(back_button, "clicked", G_CALLBACK(on_back_clicked), NULL);


    search_entry = gtk_entry_new();
    search_item = gtk_tool_item_new();
    gtk_container_add(GTK_CONTAINER(search_item), search_entry);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), search_item, -1);

    // Search Button
     search_button = gtk_tool_button_new(NULL, "Search");
     GdkPixbuf *search_pixbuf = gdk_pixbuf_new_from_file("search.png", NULL);
     if (search_pixbuf) {
         search_pixbuf = gdk_pixbuf_scale_simple(search_pixbuf, ICON_SIZE, ICON_SIZE, GDK_INTERP_BILINEAR);
         gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(search_button), gtk_image_new_from_pixbuf(search_pixbuf));
         g_object_unref(search_pixbuf); // Free the pixbuf
     } else {
         fprintf(stderr, "Failed to load icon: search.png\n");
     }
     gtk_toolbar_insert(GTK_TOOLBAR(toolbar), search_button, -1);
     g_signal_connect(search_button, "clicked", G_CALLBACK(on_search), search_entry);


    // Create tree view
    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_grid_attach(GTK_GRID(grid), scrolled_window, 0, 1, 1, 1);
    gtk_widget_set_hexpand(scrolled_window, TRUE);
    gtk_widget_set_vexpand(scrolled_window, TRUE);

    store = gtk_list_store_new(3, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING);
    tree_view = GTK_TREE_VIEW(gtk_tree_view_new_with_model(GTK_TREE_MODEL(store)));
    g_object_unref(store);

    renderer = gtk_cell_renderer_pixbuf_new();
    column = gtk_tree_view_column_new_with_attributes("Icon", renderer, "pixbuf", 0, NULL);
    gtk_tree_view_append_column(tree_view, column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Name", renderer, "text", 1, NULL);
    gtk_tree_view_append_column(tree_view, column);

    column = gtk_tree_view_column_new_with_attributes("Type", renderer, "text", 2, NULL);
    gtk_tree_view_append_column(tree_view, column);

    gtk_container_add(GTK_CONTAINER(scrolled_window), GTK_WIDGET(tree_view));

    // Connect double-click signal
    g_signal_connect(tree_view, "row-activated", G_CALLBACK(on_file_clicked), NULL);

    // Initialize the file system
    initialize_filesystem();
    current_directory = create_root_dir();
    root_directory = create_root_dir();

    // Populate the initial view
    refresh_file_list();

    GdkPixbuf *original_folder_pixbuf = gdk_pixbuf_new_from_file("folder.png", NULL);
    GdkPixbuf *original_file_pixbuf = gdk_pixbuf_new_from_file("file.png", NULL);

    if (!original_folder_pixbuf || !original_file_pixbuf) {
        show_error_dialog("Failed to load icons.");
        // Handle the error appropriately
        return;
    }

    folder_pixbuf = resize_pixbuf(original_folder_pixbuf, ICON_SIZE, ICON_SIZE);
    file_pixbuf = resize_pixbuf(original_file_pixbuf, ICON_SIZE, ICON_SIZE);

    // Free the original pixbufs as we don't need them anymore
    g_object_unref(original_folder_pixbuf);
    g_object_unref(original_file_pixbuf);

    if (!folder_pixbuf || !file_pixbuf) {
        show_error_dialog("Failed to resize icons.");
        // Handle the error appropriately
        return;
    }

    gtk_widget_show_all(window);
}

int main(int argc, char **argv) {
    GtkApplication *app;
    int status;

    app = gtk_application_new("org.example.filesystem_gui", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    return status;
}
