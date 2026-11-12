#include <gtest/gtest.h>
#include <sodium.h>

int main(int argc, char** argv) {
    if (sodium_init() < 0) {
        return 1;
    }
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
