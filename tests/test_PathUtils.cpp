#include <gtest/gtest.h>

#include "settings/PathUtils.h"

using namespace winindex;

// FormatFileCount

TEST(FormatFileCount, Zero) {
    EXPECT_EQ(FormatFileCount(0), L"0");
}

TEST(FormatFileCount, BelowThousand) {
    EXPECT_EQ(FormatFileCount(999), L"999");
}

TEST(FormatFileCount, ExactThousand) {
    EXPECT_EQ(FormatFileCount(1000), L"1,000");
}

TEST(FormatFileCount, Millions) {
    EXPECT_EQ(FormatFileCount(1234567), L"1,234,567");
}

TEST(FormatFileCount, LargeCount) {
    EXPECT_EQ(FormatFileCount(10000000), L"10,000,000");
}

// FormatAge

TEST(FormatAge, JustIndexed) {
    EXPECT_EQ(FormatAge(0), L"just indexed");
    EXPECT_EQ(FormatAge(59), L"just indexed");
}

TEST(FormatAge, Minutes) {
    EXPECT_EQ(FormatAge(60), L"1 min old");
    EXPECT_EQ(FormatAge(90), L"1 min old");
    EXPECT_EQ(FormatAge(3599), L"59 min old");
}

TEST(FormatAge, Hours) {
    EXPECT_EQ(FormatAge(3600), L"1 hrs old");
    EXPECT_EQ(FormatAge(7200), L"2 hrs old");
    EXPECT_EQ(FormatAge(172799), L"47 hrs old");
}

TEST(FormatAge, DaysAndHours) {
    EXPECT_EQ(FormatAge(172800), L"2 days, 0 hrs old");
    EXPECT_EQ(FormatAge(172800 + 3600 * 3), L"2 days, 3 hrs old");
    EXPECT_EQ(FormatAge(86400 * 6 + 3600 * 14), L"6 days, 14 hrs old");
}

// FormatLocationList

TEST(FormatLocationList, Empty) {
    EXPECT_EQ(FormatLocationList({}), L"");
}

TEST(FormatLocationList, DriveRoot) {
    EXPECT_EQ(FormatLocationList({L"C:\\"}), L"C:");
}

TEST(FormatLocationList, MultipleRoots) {
    EXPECT_EQ(FormatLocationList({L"C:\\", L"D:\\"}), L"C:, D:");
}

TEST(FormatLocationList, FolderPath) {
    EXPECT_EQ(FormatLocationList({L"E:\\projects\\"}), L"E:\\projects");
}

TEST(FormatLocationList, MixedDriveAndFolder) {
    EXPECT_EQ(FormatLocationList({L"C:\\", L"E:\\docs"}), L"C:, E:\\docs");
}

TEST(FormatLocationList, FolderNoTrailingSlash) {
    EXPECT_EQ(FormatLocationList({L"D:\\music"}), L"D:\\music");
}
