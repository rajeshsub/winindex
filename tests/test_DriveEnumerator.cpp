#include <gtest/gtest.h>
#include "indexer/DriveEnumerator.h"

using namespace winindex;

TEST(DriveEnumeratorTest, EnumeratesAtLeastOneDrive) {
    auto drives = EnumerateLocalFixedDrives();
    EXPECT_FALSE(drives.empty());
}

TEST(DriveEnumeratorTest, AllDrivesHaveRoot) {
    auto drives = EnumerateLocalFixedDrives();
    for (const auto& d : drives) {
        EXPECT_FALSE(d.root.empty());
        EXPECT_EQ(d.root.back(), L'\\');
    }
}

TEST(DriveEnumeratorTest, AllDrivesAreSupportedFilesystem) {
    auto drives = EnumerateLocalFixedDrives();
    for (const auto& d : drives) {
        EXPECT_NE(d.filesystem, DriveFilesystem::Other)
            << "Drive " << d.root.c_str() << " has unsupported filesystem";
    }
}

TEST(DriveEnumeratorTest, GetFilesystemForSystemDrive) {
    // C: is always NTFS on a modern Windows install
    auto fs = GetFilesystem(L"C:\\");
    EXPECT_EQ(fs, DriveFilesystem::NTFS);
}
