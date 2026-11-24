#include "RadioFlightTester.hpp"

TEST(RadioFlight, FrameFanoutAndBoundedQueue) {
    Deployments::RadioFlightTester tester;
    tester.testFrames();
}

TEST(RadioFlight, RadioFailureRetriesAfterResume) {
    Deployments::RadioFlightTester tester;
    tester.testRadioFailureAndResume();
}

TEST(RadioFlight, UartReturnStatusAndUnsupportedRadioInput) {
    Deployments::RadioFlightTester tester;
    tester.testUartReturnAndStatus();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
