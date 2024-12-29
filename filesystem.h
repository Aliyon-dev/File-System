#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// File System Definitions

#define MAX_BLOCKS 1024
#define MAX_INODES 256
#define BLOCK_SIZE 64
#define FILE_NAME_LENGTH 255
#define INDEX_BLOCK_SIZE 12
#define MAX_OPEN_FILES 100
#define BUFFER_SIZE 64
#define CACHE_SIZE 16
#define JOURNAL_SIZE 100

// Data Structures

// Superblock definition
typedef struct {
    int total_blocks;
    int free_blocks;
    int block_size;
    int inode_count;
    int free_inodes;
} superblock;

// Inode definition
typedef struct {
    int inode_number;
    int file_size;
    char file_type;
    int permissions;
    int owner;
    int timestamps[3];
    int data_blocks[INDEX_BLOCK_SIZE];
} inode;

// Directory Entry definition
typedef struct {
    char name[FILE_NAME_LENGTH];
    int inode_number;
} DirectoryEntry;

// Directory definition
typedef struct {
    DirectoryEntry entries[MAX_INODES];
} Directory;

// Open file definition
typedef struct {
    int inode_number;
    int current_position;
} OpenFile;

// Permissions definition
typedef struct {
    unsigned char read;
    unsigned char write;
    unsigned char execute;
} Permissions;

// Hierarchical Directory Structure
typedef struct DirectoryStruct {
    char name[100];
    struct DirectoryStruct* parent;
    struct DirectoryStruct** children;
    int child_count;
    int max_children;
    Permissions permissions;
    int is_directory;  // New field: 1 for directory, 0 for file
    int inode_number;  // Add this to link with the file system's inode
} DirectoryStruct;

// Buffer cache structure
typedef struct {
    int block_num;
    char data[BLOCK_SIZE];
    int dirty;
    int last_used;
} CacheBlock;

// Journal entry structure
typedef struct {
    int operation; // 0: write, 1: create, 2: delete, 3: rename
    int block_num;
    char data[BLOCK_SIZE];
    int file_size;
    char filename[FILE_NAME_LENGTH];
    char old_filename[FILE_NAME_LENGTH];
    char new_filename[FILE_NAME_LENGTH];
} JournalEntry;


// Global Variables

superblock sb;
inode inodes[MAX_INODES];
Directory directory;
OpenFile open_files[MAX_OPEN_FILES];
unsigned char blocks[MAX_BLOCKS * BLOCK_SIZE];
unsigned char block_bitmap[MAX_BLOCKS / 8];
CacheBlock cache[CACHE_SIZE];
int cache_clock = 0;
JournalEntry journal[JOURNAL_SIZE];
int journal_index = 0;

// Cache Initialization

void init_cache() {
    for (int i = 0; i < CACHE_SIZE; i++) {
        cache[i].block_num = -1;
        cache[i].dirty = 0;
        cache[i].last_used = 0;
    }
}

// Cache Functions

// Function to get a block from cache or disk
char* get_block(int block_num) {
    // Check if block is in cache
    for (int i = 0; i < CACHE_SIZE; i++) {
        if (cache[i].block_num == block_num) {
            cache[i].last_used = cache_clock++;
            return cache[i].data;
        }
    }

    // If not in cache, load from disk
    int lru_index = 0;
    for (int i = 1; i < CACHE_SIZE; i++) {
        if (cache[i].last_used < cache[lru_index].last_used) {
            lru_index = i;
        }
    }

    // If dirty, write back to disk
    if (cache[lru_index].dirty) {
        memcpy(&blocks[cache[lru_index].block_num * BLOCK_SIZE], cache[lru_index].data, BLOCK_SIZE);
    }

    // Load new block into cache
    cache[lru_index].block_num = block_num;
    memcpy(cache[lru_index].data, &blocks[block_num * BLOCK_SIZE], BLOCK_SIZE);
    cache[lru_index].dirty = 0;
    cache[lru_index].last_used = cache_clock++;

    return cache[lru_index].data;
}

// Function to write a block to cache
void write_block(int block_num, const char* data) {
    char* cache_data = get_block(block_num);
    memcpy(cache_data, data, BLOCK_SIZE);

    // Find the cache entry and mark it as dirty
    for (int i = 0; i < CACHE_SIZE; i++) {
        if (cache[i].block_num == block_num) {
            cache[i].dirty = 1;
            break;
        }
    }

    // Add to journal
    journal[journal_index].operation = 0; // write operation
    journal[journal_index].block_num = block_num;
    memcpy(journal[journal_index].data, data, BLOCK_SIZE);
    journal_index = (journal_index + 1) % JOURNAL_SIZE;
}

// Function to flush cache to disk
void flush_cache() {
    for (int i = 0; i < CACHE_SIZE; i++) {
        if (cache[i].dirty) {
            memcpy(&blocks[cache[i].block_num * BLOCK_SIZE], cache[i].data, BLOCK_SIZE);
            cache[i].dirty = 0;
        }
    }
}

// Directory Functions

// Create the root directory
DirectoryStruct* create_root_dir() {
    DirectoryStruct* root = (DirectoryStruct*)malloc(sizeof(DirectoryStruct));
    if (root == NULL) {
        printf("Error: Memory allocation failed for root directory\n");
        return NULL;
    }

    strcpy(root->name, "root");
    root->parent = NULL;
    root->children = NULL;
    root->child_count = 0;
    root->max_children = 0;
    root->permissions = (Permissions){1, 1, 1}; // Default permissions: read, write, execute

    return root;
}

// Create a new directory
DirectoryStruct* create_dir(const char* dir_name, DirectoryStruct* parent) {
    DirectoryStruct* dir = (DirectoryStruct*)malloc(sizeof(DirectoryStruct));
    strcpy(dir->name, dir_name);
    dir->parent = parent;
    dir->children = NULL;
    dir->child_count = 0;
    dir->max_children = 0;
    dir->permissions = (Permissions){1, 1, 1}; // Default permissions

    if (parent) {
        if (parent->child_count >= parent->max_children) {
            parent->max_children = parent->max_children ? parent->max_children * 2 : 1;
            parent->children = realloc(parent->children, parent->max_children * sizeof(DirectoryStruct*));
        }
        parent->children[parent->child_count++] = dir;
    }

    return dir;
}

// Find directory by name
DirectoryStruct* find_directory(DirectoryStruct* parent, const char* dir_name) {
    if (parent == NULL || dir_name == NULL) {
        return NULL;
    }

    for (int i = 0; i < parent->child_count; i++) {
        if (strcmp(parent->children[i]->name, dir_name) == 0) {
            return parent->children[i];
        }
    }

    return NULL;
}

// Get the file size
int get_file_size(const char *filename) {
    for (int i = 0; i < MAX_INODES; i++) {
        if (directory.entries[i].inode_number != -1 && strcmp(directory.entries[i].name, filename) == 0) {
            int inode_number = directory.entries[i].inode_number;
            if (inode_number >= 0 && inode_number < MAX_INODES) {
                return inodes[inode_number].file_size;
            } else {
                printf("Error: Invalid inode number %d for file %s\n", inode_number, filename);
                return -1;
            }
        }
    }
    printf("Error: File %s not found\n", filename);
    return -1; // Or handle error as appropriate in your context
}

// List all directories and subdirectories
void list_directory(DirectoryStruct* dir, int depth) {
    for (int i = 0; i < dir->child_count; i++) {
        for (int j = 0; j < depth; j++) {
            printf("  ");
        }
        printf("[DIR] %s\n", dir->children[i]->name);
        list_directory(dir->children[i], depth + 1);
    }
}


// Navigate directory path
DirectoryStruct* navigate_path(DirectoryStruct* root, const char* path) {
    char* path_copy = strdup(path);
    char* token = strtok(path_copy, "/");
    DirectoryStruct* current = root;

    while (token != NULL) {
        DirectoryStruct* next = find_directory(current, token);
        if (next == NULL) {
            free(path_copy);
            return NULL;
        }
        current = next;
        token = strtok(NULL, "/");
    }

    free(path_copy);
    return current;
}


// Calculate directory size
int calculate_directory_size(DirectoryStruct* dir) {
    int total_size = 0;
    for (int i = 0; i < dir->child_count; i++) {
        if (dir->children[i]->child_count > 0) {
            // This is a subdirectory
            total_size += calculate_directory_size(dir->children[i]);
        } else {
            // This is a file
            int file_size = get_file_size(dir->children[i]->name);
            if (file_size >= 0)
            {
                total_size += file_size;
            }
        }
    }
    return total_size;
}


// Set directory permissions
void set_directory_permissions(DirectoryStruct* dir, unsigned char read, unsigned char write, unsigned char execute) {
    dir->permissions.read = read;
    dir->permissions.write = write;
    dir->permissions.execute = execute;
}

// Block Functions

// Allocate a block
int allocate_block() {
    for (int i = 0; i < MAX_BLOCKS; i++) {
        if (!(block_bitmap[i / 8] & (1 << (i % 8)))) {
            block_bitmap[i / 8] |= (1 << (i % 8));
            sb.free_blocks--;
            return i;
        }
    }
    return -1;
}

// Free a block
void free_block(int block_num) {
    block_bitmap[block_num / 8] &= ~(1 << (block_num % 8));
    sb.free_blocks++;
}

// File operations

// Create a file
int create_file(const char *filename, int size, int permissions) {
    int blocks_needed = (size + sb.block_size - 1) / sb.block_size;
    if (blocks_needed > INDEX_BLOCK_SIZE) {
        printf("Error: File size too large for current implementation\n");
        return -1;
    }
    if (sb.free_blocks < blocks_needed || sb.free_inodes == 0) {
        printf("Error: Not enough free space or inodes to create file %s\n", filename);
        return -1;
    }
    int block_nums[INDEX_BLOCK_SIZE];
    for (int i = 0; i < blocks_needed; i++) {
        block_nums[i] = allocate_block();
        if (block_nums[i] == -1) {
            for (int j = 0; j < i; j++) {
                free_block(block_nums[j]);
            }
            printf("Error: Failed to allocate blocks for file %s\n", filename);
            return -1;
        }
    }
    int inode_number = -1;
    for (int i = 0; i < MAX_INODES; i++) {
        if (inodes[i].inode_number == -1) {
            inode_number = i;
            break;
        }
    }
    if (inode_number == -1) {
        for (int i = 0; i < blocks_needed; i++) {
            free_block(block_nums[i]);
        }
        printf("Error: No free inodes available\n");
        return -1;
    }
    inodes[inode_number].inode_number = inode_number;
    inodes[inode_number].file_size = size;
    inodes[inode_number].permissions = permissions;
    memcpy(inodes[inode_number].data_blocks, block_nums, sizeof(block_nums));
    for (int i = 0; i < MAX_INODES; i++) {
        if (directory.entries[i].inode_number == -1) {
            strncpy(directory.entries[i].name, filename, FILE_NAME_LENGTH - 1);
            directory.entries[i].name[FILE_NAME_LENGTH - 1] = '\0';
            directory.entries[i].inode_number = inode_number;
            break;
        }
    }
    sb.free_inodes--;
    return inode_number;
}

// Delete a file
int delete_file(const char *filename) {
    for (int i = 0; i < MAX_INODES; i++) {
        if (directory.entries[i].inode_number != -1 && strcmp(directory.entries[i].name, filename) == 0) {
            int inode_number = directory.entries[i].inode_number;
            if (inode_number < 0 || inode_number >= MAX_INODES) {
                printf("Error: Invalid inode number %d for file %s\n", inode_number, filename);
                return -1;
            }
            for (int j = 0; j < INDEX_BLOCK_SIZE; j++) {
                int block_num = inodes[inode_number].data_blocks[j];
                if (block_num >= 0 && block_num < MAX_BLOCKS) {
                    free_block(block_num);
                }
                inodes[inode_number].data_blocks[j] = -1;
            }
            inodes[inode_number].inode_number = -1;
            inodes[inode_number].file_size = 0;
            directory.entries[i].inode_number = -1;
            memset(directory.entries[i].name, 0, FILE_NAME_LENGTH);
            sb.free_inodes++;
            return 0;
        }
    }
}

// Open a file
int open_file(const char *filename) {
    for (int i = 0; i < MAX_INODES; i++) {
        if (directory.entries[i].inode_number != -1 && strcmp(directory.entries[i].name, filename) == 0) {
            int inode_number = directory.entries[i].inode_number;
            for (int j = 0; j < MAX_OPEN_FILES; j++) {
                if (open_files[j].inode_number == -1) {
                    open_files[j].inode_number = inode_number;
                    open_files[j].current_position = 0;
                    inodes[inode_number].timestamps[2] = time(NULL);  // Update access time
                    return j;  // Return file descriptor
                }
            }
            return -1;  // Too many open files
        }
    }
    return -1;  // File not found
}

// Close a file
void close_file(int file_descriptor) {
    if (file_descriptor >= 0 && file_descriptor < MAX_OPEN_FILES) {
        if (open_files[file_descriptor].inode_number != -1) {
            open_files[file_descriptor].inode_number = -1;
            open_files[file_descriptor].current_position = 0;
            printf("File descriptor %d closed successfully\n", file_descriptor);
        } else {
            printf("Error: File descriptor %d is not open\n", file_descriptor);
        }
    } else {
        printf("Error: Invalid file descriptor %d\n", file_descriptor);
    }
}

// Function to set file permissions
void set_permissions(int inode_num, int permissions) {
    if (inode_num >= 0 && inode_num < MAX_INODES) {
        inodes[inode_num].permissions = permissions;
    }
}

// Function to check file permissions
int check_permissions(int inode_num, int requested_permission) {
    if (inode_num >= 0 && inode_num < MAX_INODES) {
        return (inodes[inode_num].permissions & requested_permission) != 0;
    }
    return 0;
}

// Read from a file
int read_file(int file_descriptor, char *buffer, int size) {
    if (file_descriptor < 0 || file_descriptor >= MAX_OPEN_FILES || open_files[file_descriptor].inode_number == -1) {
        return -1;
    }
    int inode_number = open_files[file_descriptor].inode_number;

    // Check read permissions
    if (!check_permissions(inode_number, 4)) { // 4 is read permission
        printf("Error: No read permission for file\n");
        return -1;
    }

    int current_position = open_files[file_descriptor].current_position;
    int file_size = inodes[inode_number].file_size;
    int bytes_to_read = (current_position + size > file_size) ? (file_size - current_position) : size;
    int bytes_read = 0;

    while (bytes_read < bytes_to_read) {
        int block_index = current_position / BLOCK_SIZE;
        int block_offset = current_position % BLOCK_SIZE;
        int block_number = inodes[inode_number].data_blocks[block_index];
        int bytes_from_block = BLOCK_SIZE - block_offset;
        if (bytes_from_block > bytes_to_read - bytes_read) {
            bytes_from_block = bytes_to_read - bytes_read;
        }

        // Use get_block to access data through cache
        char* block_data = get_block(block_number);
        memcpy(buffer + bytes_read, block_data + block_offset, bytes_from_block);

        bytes_read += bytes_from_block;
        current_position += bytes_from_block;
    }

    open_files[file_descriptor].current_position = current_position;
    inodes[inode_number].timestamps[2] = time(NULL);  // Update access time
    return bytes_read;
}

// Write to a file
int write_file(int file_descriptor, const char *buffer, int size) {
    if (file_descriptor < 0 || file_descriptor >= MAX_OPEN_FILES || open_files[file_descriptor].inode_number == -1) {
        return -1;
    }
    int inode_number = open_files[file_descriptor].inode_number;

    // Check write permissions
    if (!check_permissions(inode_number, 2)) { // 2 is write permission
        printf("Error: No write permission for file\n");
        return -1;
    }

    int current_position = open_files[file_descriptor].current_position;
    int bytes_written = 0;

    while (bytes_written < size) {
        int block_index = current_position / BLOCK_SIZE;
        int block_offset = current_position % BLOCK_SIZE;
        if (inodes[inode_number].data_blocks[block_index] == -1) {
            int new_block = allocate_block();
            if (new_block == -1) {
                break;
            }
            inodes[inode_number].data_blocks[block_index] = new_block;
        }
        int block_number = inodes[inode_number].data_blocks[block_index];
        int bytes_to_write = BLOCK_SIZE - block_offset;
        if (bytes_to_write > size - bytes_written) {
            bytes_to_write = size - bytes_written;
        }

        // Use write_block to write data through cache
        char* block_data = get_block(block_number);
        memcpy(block_data + block_offset, buffer + bytes_written, bytes_to_write);
        write_block(block_number, block_data);

        bytes_written += bytes_to_write;
        current_position += bytes_to_write;
        if (current_position > inodes[inode_number].file_size) {
            inodes[inode_number].file_size = current_position;
        }
    }

    open_files[file_descriptor].current_position = current_position;
    inodes[inode_number].timestamps[1] = time(NULL);  // Update modification time
    return bytes_written;
}

// Rename a file
int rename_file(const char *old_name, const char *new_name) {
    int old_index = -1;
    int new_index = -1;

    // Find the old file and an empty slot for the new name
    for (int i = 0; i < MAX_INODES; i++) {
        if (directory.entries[i].inode_number != -1 && strcmp(directory.entries[i].name, old_name) == 0) {
            old_index = i;
        }
        if (directory.entries[i].inode_number == -1 && new_index == -1) {
            new_index = i;
        }
        if (directory.entries[i].inode_number != -1 && strcmp(directory.entries[i].name, new_name) == 0) {
            printf("Error: File with name %s already exists\n", new_name);
            return -1;
        }
    }

    if (old_index == -1) {
        printf("Error: File %s not found\n", old_name);
        return -1;
    }

    if (new_index == -1) {
        printf("Error: No free directory entries for the new name\n");
        return -1;
    }

    // Perform the rename
    strncpy(directory.entries[new_index].name, new_name, FILE_NAME_LENGTH - 1);
    directory.entries[new_index].name[FILE_NAME_LENGTH - 1] = '\0';
    directory.entries[new_index].inode_number = directory.entries[old_index].inode_number;

    // Clear the old entry
    directory.entries[old_index].inode_number = -1;
    memset(directory.entries[old_index].name, 0, FILE_NAME_LENGTH);

    printf("File renamed from %s to %s successfully\n", old_name, new_name);
    return 0;
}

// Recovery from journal Function
void recover_from_journal() {
    int permissions = 7;
    printf("Recovering file system state from journal...\n");
    for (int i = 0; i < JOURNAL_SIZE; i++) {
        if (journal[i].operation == 0) { // write operation
            memcpy(&blocks[journal[i].block_num * BLOCK_SIZE], journal[i].data, BLOCK_SIZE);
        }
        else if (journal[i].operation == 1) { // create operation,
            create_file(journal[i].filename, journal[i].file_size, 7);
        }
        else if (journal[i].operation == 2) { // delete operation
            delete_file(journal[i].filename);
        }
        else if (journal[i].operation == 3) { // rename operation
            rename_file(journal[i].old_filename, journal[i].new_filename);
        }
        else
        {
            printf("Unknown operation in journal entry %d\n", i);
        }
    }
    printf("Recovery complete.\n");
}

int rename_directory(DirectoryStruct* root, const char* old_path, const char* new_name) {
    // Find the directory to be renamed
    DirectoryStruct* dir = navigate_path(root, old_path);
    if (dir == NULL) {
        printf("Error: Directory %s not found\n", old_path);
        return -1;
    }

    // Check if the directory is the root directory
    if (dir->parent == NULL) {
        printf("Error: Cannot rename root directory\n");
        return -1;
    }

    // Check if a directory or file with the new name already exists in the parent directory
    DirectoryStruct* parent = dir->parent;
    for (int i = 0; i < parent->child_count; i++) {
        if (strcmp(parent->children[i]->name, new_name) == 0) {
            printf("Error: A directory or file with name %s already exists\n", new_name);
            return -1;
        }
    }

    // Rename the directory
    strncpy(dir->name, new_name, sizeof(dir->name) - 1);
    dir->name[sizeof(dir->name) - 1] = '\0';  // Ensure null-termination

    // Update the journal
    journal[journal_index].operation = 3; // rename operation
    strncpy(journal[journal_index].old_filename, old_path, FILE_NAME_LENGTH - 1);
    journal[journal_index].old_filename[FILE_NAME_LENGTH - 1] = '\0';
    strncpy(journal[journal_index].new_filename, new_name, FILE_NAME_LENGTH - 1);
    journal[journal_index].new_filename[FILE_NAME_LENGTH - 1] = '\0';
    journal_index = (journal_index + 1) % JOURNAL_SIZE;

    printf("Directory renamed from %s to %s successfully\n", old_path, new_name);
    return 0;
}

// Delete Directory
void delete_directory(DirectoryStruct *dir) {
    if (dir == NULL) return;


    for (int i = 0; i < dir->child_count; i++) {
        if (dir->children[i]->is_directory) {
            delete_directory(dir->children[i]);
        } else {
            delete_file(dir->children[i]->name);
        }
        free(dir->children[i]);
    }

    free(dir->children);


    if (dir->parent) {
        for (int i = 0; i < dir->parent->child_count; i++) {
            if (dir->parent->children[i] == dir) {
                // Shift remaining children
                for (int j = i; j < dir->parent->child_count - 1; j++) {
                    dir->parent->children[j] = dir->parent->children[j + 1];
                }
                dir->parent->child_count--;
                break;
            }
        }
    }

    // Free the directory struct itself
    free(dir);
}

void delete_node(DirectoryStruct* node) {
    if (node == NULL) {
        return;
    }

    // If it's a directory, recursively delete all children
    if (node->is_directory) {
        for (int i = 0; i < node->child_count; i++) {
            delete_node(node->children[i]);
        }
        free(node->children);
    } else {
        // If it's a file, delete the associated inode and free blocks
        delete_file(node->name);
    }

    // Remove from parent's children list
    if (node->parent) {
        for (int i = 0; i < node->parent->child_count; i++) {
            if (node->parent->children[i] == node) {
                // Shift remaining children
                for (int j = i; j < node->parent->child_count - 1; j++) {
                    node->parent->children[j] = node->parent->children[j + 1];
                }
                node->parent->child_count--;
                break;
            }
        }
    }

    // Free the node itself
    free(node);
}

// Wrapper function to delete by path
int delete_by_path(DirectoryStruct* root, const char* path) {
    DirectoryStruct* node = navigate_path(root, path);
    if (node == NULL) {
        printf("Error: Path %s not found\n", path);
        return -1;
    }

    delete_node(node);
    printf("%s deleted successfully\n", path);
    return 0;
}

// File System Initialization

void initialize_filesystem() {
    sb.total_blocks = MAX_BLOCKS;
    sb.free_blocks = MAX_BLOCKS;
    sb.block_size = BLOCK_SIZE;
    sb.inode_count = MAX_INODES;
    sb.free_inodes = MAX_INODES;
    memset(block_bitmap, 0, sizeof(block_bitmap));
    memset(blocks, 0, sizeof(blocks));
    init_cache();
    recover_from_journal();
    for (int i = 0; i < MAX_INODES; i++) {
        inodes[i].inode_number = -1;
        memset(inodes[i].data_blocks, -1, sizeof(inodes[i].data_blocks));
    }
    for (int i = 0; i < MAX_INODES; i++) {
        directory.entries[i].inode_number = -1;
    }
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        open_files[i].inode_number = -1;
        open_files[i].current_position = 0;
    }
}
