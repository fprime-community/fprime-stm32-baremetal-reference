#include "RadioTestTester.hpp"

TEST(RadioTest, Parameters) {
    ReferenceDeployment::RadioTestTester tester;
    tester.testParameters();
}

TEST(RadioTest, Reply) {
    ReferenceDeployment::RadioTestTester tester;
    tester.testReply();
}

TEST(RadioTest, SendString) {
    ReferenceDeployment::RadioTestTester tester;
    tester.testSendString();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
