#include <gtest/gtest.h>
#include <sodium.h>
#include <QApplication>

// Widgets need a QApplication, which must be created before any test runs.
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    if (sodium_init() < 0) {
        return 1;
    }
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
