#include <iostream>
#include <fstream>
#include <filesystem>

int main() {
    try {
        std::filesystem::path src = "d:\\File\\AR\\COMGRAPH_updated.zip";
        std::filesystem::path dest = "COMGRAPH_updated.zip";
        if (std::filesystem::exists(src)) {
            std::filesystem::copy_file(src, dest, std::filesystem::copy_options::overwrite_existing);
            std::cout << "SUCCESS: Copied zip file to workspace." << std::endl;
        } else {
            std::cerr << "ERROR: Source file does not exist." << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
    }
    return 0;
}
