#include "RadioTestTester.hpp"

TEST(RadioTest, Parameters) {
    ReferenceDeployment::RadioTestTester tester;
    tester.testParameters();
}

TEST(RadioTest, Reply) {
    ReferenceDeployment::RadioTestTester tester;
    tester.testReply();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
