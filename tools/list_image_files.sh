#!/bin/bash

list_image_files() {
    if [ "$#" -ne 1 ]; then
        echo "Usage: list_image_files src_path"
        return 1
    fi

    local src_dir="$1"

    if [ ! -d "$src_dir" ]; then
        echo "Source path '$src_dir' not exists !!!"
        return 1
    fi

    find "$src_dir" -type f -name "*.exe" -exec basename {} \;
}

export -f list_image_files