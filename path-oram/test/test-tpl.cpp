#include <gtest/gtest.h>
#include <fstream>
#include <iostream>
#include <string>
#include <cstdlib>
#include <cstdio>

// Utility function to create a sample file for testing
void createSampleFile(const std::string& filepath, const std::string& content) {
    std::ofstream outfile(filepath);
    outfile << content;
    outfile.close();
}

// Utility function to read the contents of a file
std::string readFileContent(const std::string& filepath) {
    std::ifstream infile(filepath);
    std::string content((std::istreambuf_iterator<char>(infile)),
                        std::istreambuf_iterator<char>());
    infile.close();
    return content;
}

TEST(TrustedProxyLayerTest, SecretSharingAndDecryption) {
    // Define paths
    const std::string inputFile = "~/Desktop/Secret_File.txt";
    const std::string decryptedFile = "~/Documents/decrypted.txt";

    // Create sample input file
    std::string originalContent = "This is a test content for secret sharing.";
    createSampleFile(inputFile, originalContent);

    // Perform encryption
    std::string encryptCommand = "./main -encrypt \"" + inputFile + "\"";
    int encryptResult = std::system(encryptCommand.c_str());
    ASSERT_EQ(encryptResult, 0) << "Encryption command failed.";

    // Perform decryption
    std::string decryptCommand = "./main -decrypt \"" + decryptedFile + "\"";
    int decryptResult = std::system(decryptCommand.c_str());
    ASSERT_EQ(decryptResult, 0) << "Decryption command failed.";

    // Verify the decrypted file content
    std::string decryptedContent = readFileContent(decryptedFile);
    ASSERT_EQ(decryptedContent, originalContent)
        << "Decrypted content does not match the original.";

    // Cleanup (optional, depending on your test environment)
    std::remove(inputFile.c_str());
    std::remove(decryptedFile.c_str());
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
