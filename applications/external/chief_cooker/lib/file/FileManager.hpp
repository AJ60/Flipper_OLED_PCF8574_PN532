#pragma once

#include "lib/String.hpp"
#include <toolbox/path.h>
#include <storage/storage.h>

#include "FlipperFile.hpp"
#include "Directory.hpp"

class FileManager {
private:
    Storage* storage = nullptr;

public:
    FileManager() {
        storage = (Storage*)furi_record_open(RECORD_STORAGE);
    }

    FileManager(const FileManager&) = delete;
    FileManager& operator=(const FileManager&) = delete;

    FileManager(FileManager&& other) noexcept : storage(other.storage) {
        other.storage = nullptr;
    }

    FileManager& operator=(FileManager&& other) noexcept {
        if(this != &other) {
            if(storage) {
                furi_record_close(RECORD_STORAGE);
            }
            storage = other.storage;
            other.storage = nullptr;
        }
        return *this;
    }

    Storage* GetStorage() {
        return storage;
    }

    bool DirExists(const char* path) {
        if(!storage || !path) return false;
        return storage_dir_exists(storage, path);
    }

    void CreateDirIfNotExists(const char* path) {
        if(!storage || !path) return;
        if(!storage_dir_exists(storage, path)) {
            storage_common_mkdir(storage, path);
        }
    }

    Directory* OpenDirectory(const char* path) {
        if(!storage || !path) return NULL;
        if(!storage_dir_exists(storage, path)) {
            return NULL;
        }
        Directory* dir = new Directory(storage, path);
        if(dir->IsOpened()) {
            return dir;
        }
        delete dir;
        return NULL;
    }

    FlipperFile* OpenRead(const char* path) {
        if(!storage || !path) return NULL;
        FlipperFile* file = new FlipperFile(storage, path, false);
        if(file->IsOpened()) {
            return file;
        }
        delete file;
        return NULL;
    }

    FlipperFile* OpenRead(const char* dir, const char* file) {
        String concatedPath = String("%s/%s", dir, file);
        return OpenRead(concatedPath.cstr());
    }

    FlipperFile* OpenWrite(const char* path) {
        if(!storage || !path) return NULL;
        FlipperFile* file = new FlipperFile(storage, path, true);
        if(file->IsOpened()) {
            return file;
        }
        delete file;
        return NULL;
    }

    FlipperFile* OpenWrite(const char* dir, const char* file) {
        String concatedPath = String("%s/%s", dir, file);
        return OpenWrite(concatedPath.cstr());
    }

    void DeleteFile(const char* dir, const char* file) {
        String concatedPath = String("%s/%s", dir, file);
        DeleteFile(concatedPath.cstr());
    }

    void DeleteFile(const char* filePath) {
        if(storage && filePath) {
            storage_common_remove(storage, filePath);
        }
    }

    ~FileManager() {
        if(storage) {
            furi_record_close(RECORD_STORAGE);
            storage = nullptr;
        }
    }
};
