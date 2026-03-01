#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>
#include <zlib.h>
#include <openssl/sha.h>

int main(int argc, char *argv[])
{
    // Flush after every std::cout / std::cerr
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    // You can use print statements as follows for debugging, they'll be visible when running tests.
    std::cerr << "Logs from your program will appear here!\n";
    if (argc <= 1)
    {
        std::cerr << "No command provided.\n";
        return EXIT_FAILURE;
    }
    std::string command = argv[1];
    std::cerr << "Command " << command << std::endl;
    
    if (command == "init")
    {
        try
        {
            std::filesystem::create_directory(".git");
            std::filesystem::create_directory(".git/objects");
            std::filesystem::create_directory(".git/refs");
    
            std::ofstream headFile(".git/HEAD");
            if (headFile.is_open())
            { 
                headFile << "ref: refs/heads/main\n";
                headFile.close();
            }
            else
            {
                std::cerr << "Failed to create .git/HEAD file.\n";
                return EXIT_FAILURE;
            }    
            std::cout << "Initialized git directory\n";
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            std::cerr << e.what() << '\n';
            return EXIT_FAILURE;
        }
    }
    else if (command == "cat-file")
    {
        /*command to build program g++ main.cpp -lz*/
        if (argc < 4 || std::string(argv[2]) != "-p")
        {
            std::cerr << "Usage: cat-file -p <object>\n";
            std::cerr << "Unknown command " << command <<" "<< argv[2] << '\n';
            return EXIT_FAILURE;
        }
        std::string objectHash = argv[3];
        
        std::string dir = objectHash.substr(0, 2);
        std::string file = objectHash.substr(2);
        //std::cout << "dir " << dir << " file " << file << "\n";
        std::filesystem::path objectPath = std::filesystem::path(".git/objects") / dir / file;
        if (!std::filesystem::exists(objectPath))
        {
            std::cerr << "Object " << objectHash << " not found.\n";
            return EXIT_FAILURE;
        }
        
        std::ifstream filepath(objectPath, std::ios::binary);
        if (!filepath.is_open())
        {
            std::cerr << "Error opening object " <<objectHash<< std::endl;
            return EXIT_FAILURE;
        }
        std::string compressed((std::istreambuf_iterator<char>(filepath)),
                                std::istreambuf_iterator<char>());

        // Now decompress it
        z_stream stream = {};
        if (inflateInit(&stream) != Z_OK)
        {
            throw std::runtime_error("Failed to initialize zlib");
        }
        stream.next_in = (Bytef *)compressed.data();
        stream.avail_in = compressed.size();
        std::string decompressed;
        char buffer[4096];
        do
        {
            stream.next_out = (Bytef *)buffer;
            stream.avail_out = sizeof(buffer);
            int ret = inflate(&stream, Z_NO_FLUSH);
            if (ret != Z_OK && ret != Z_STREAM_END)
            {
                inflateEnd(&stream);
                throw std::runtime_error("Decompression failed");
            }
            decompressed.append(buffer, sizeof(buffer) - stream.avail_out);
        }while (stream.avail_out == 0);
        inflateEnd(&stream);
        size_t null_pos = decompressed.find('\0');
        if (null_pos == std::string::npos)
        {
            throw std::runtime_error("Invalid git object - no null terminator");
        }
        std::string content = decompressed.substr(null_pos + 1);
        std::cout << content;
        return EXIT_SUCCESS;
    }
    else if (command == "hash-object")
    {
        /*command to build program g++ main.cpp -lz -lcrypto*/
        try
        {
            std::string filepath;
            std::string flag;
            if (argc == 4)
            {
                flag = argv[2];
                if (flag != "-w")
                {
                    std::cerr << "Invalid flag for hash-object, expected '-w'\n";
                    return EXIT_FAILURE;
                }
                filepath = argv[3];
            }
            else if (argc == 3)
            {
                filepath = argv[2];
            }
            else
            {
                std::cerr << "Invalid Arguments\n";
                return EXIT_FAILURE;
            }
            std::ifstream file(filepath, std::ios::binary);
            if (!file.is_open())
            {
                std::cerr << "Failed to open file: " << filepath << "\n";
                exit(EXIT_FAILURE);
            }
            std::string content((std::istreambuf_iterator<char>(file)),
                                 std::istreambuf_iterator<char>());
            
            //blob <size>\0<content>
            std::string object = "blob " + std::to_string(content.size()) + '\0' + content;

            unsigned char hash[20];
            SHA1(reinterpret_cast<const unsigned char *>(object.data()),object.size(),hash);

            std::stringstream ss;
            for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
            {
                ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
            }
            std::string hexHash = ss.str();
            std::cout << hexHash << std::endl;          
            if (flag == "-w")
            {
                z_stream stream{};
                deflateInit(&stream, Z_DEFAULT_COMPRESSION);
                stream.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(object.data()));
                stream.avail_in = object.size();
                std::string compressed;
                char buffer[4096];
                int ret;
                do
                {
                    stream.next_out = reinterpret_cast<Bytef *>(buffer);
                    stream.avail_out = sizeof(buffer);
                    ret = deflate(&stream, Z_FINISH);
                    compressed.append(buffer, sizeof(buffer) - stream.avail_out);
                } while (ret == Z_OK);
                deflateEnd(&stream);
    
                std::string dir = ".git/objects/" + hexHash.substr(0, 2);
                std::filesystem::create_directory(dir);
                std::ofstream out(dir + "/" + hexHash.substr(2), std::ios::binary);
                out.write(compressed.data(), compressed.size());
                out.close();
            }
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            std::cerr << e.what() << '\n';
            return EXIT_FAILURE;
        }
    }
    else
    {
        std::cerr << "Unknown command " << command << '\n';
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}
