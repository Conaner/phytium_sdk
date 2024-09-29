#!/bin/bash

copy_image_files() {
    if [ "$#" -ne 2 ]; then
        echo "Usage: copy_image_files src_path dst_path"
        return 1
    fi

    local src_dir="$1"
    local dest_dir="$2"

    if [ ! -d "$src_dir" ]; then
        echo "Source path '$src_dir' not exists !!!"
        return 1
    fi

    if [ ! -d "$dest_dir" ]; then
        echo "Destination path '$dest_dir' not exists, creating !!!"
        mkdir -p "$dest_dir"
    fi

    find "$src_dir" -type f -name "*.exe" -exec cp {} "$dest_dir" \;
    echo "Copy all .exe images from '$src_dir' to '$dest_dir'"
}

copy_files_by_name() {
    if [ "$#" -ne 3 ]; then
        echo "Usage: copy_files_by_name src_path image_name dst_path"
        return 1
    fi

    local src_dir="$1"
    local file_name="$2"
    local dest_dir="$3"

    if [ ! -d "$src_dir" ]; then
        echo "Source path '$src_dir' not exists !!!"
        return 1
    fi

    if [ ! -d "$dest_dir" ]; then
        echo "Destination path '$dest_dir' not exists, creating !!!"
        mkdir -p "$dest_dir"
    fi

    find "$src_dir" -type f -name "$file_name" -exec cp {} "$dest_dir" -f \;
    echo "Copy image '$file_name' from '$src_dir' to '$dest_dir'"
}

export -f copy_image_files
export -f copy_files_by_name