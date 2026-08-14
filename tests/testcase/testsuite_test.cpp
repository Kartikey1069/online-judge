#include <gtest/gtest.h>

#include "testcase/testsuite.hpp"


TEST(TestSuite,AddandGetTestCase) {
    TestCase actual{
        "5\n",
        "25\n"
    };
    

    TestSuite testsuite;
    testsuite.addTestCase(actual);


    const TestCase& result = testsuite.getTestCase(0);

    EXPECT_EQ(actual.input,result.input);

    EXPECT_EQ(actual.expected_output,result.expected_output);

}


TEST(TestSuite,PreservesInsertionOrder) {
    TestCase A{
        "5\n",
        "25\n"
    };

    TestCase B{
        "4\n",
        "16\n"
    };

    TestCase C{
        "30\n",
        "900\n"
    };


    TestSuite  testsuite;
    testsuite.addTestCase(A);
    testsuite.addTestCase(B);
    testsuite.addTestCase(C);


    const TestCase& resultA=testsuite.getTestCase(0);
    
    EXPECT_EQ(A.input,resultA.input);
    EXPECT_EQ(A.expected_output,resultA.expected_output);

    const TestCase& resultB=testsuite.getTestCase(1);
    
    EXPECT_EQ(B.input,resultB.input);
    EXPECT_EQ(B.expected_output,resultB.expected_output);

    const TestCase& resultC=testsuite.getTestCase(2);
    
    EXPECT_EQ(C.input,resultC.input);
    EXPECT_EQ(C.expected_output,resultC.expected_output);


}


TEST(TestSuite,InvalidIndexThrows) {
    TestCase A{
        "5\n",
        "25\n"
    };

    TestCase B{
        "4\n",
        "16\n"
    };
    TestSuite testsuite;
    testsuite.addTestCase(A);
    testsuite.addTestCase(B);

    EXPECT_THROW(testsuite.getTestCase(2),std::out_of_range);

}

TEST(TestSuite, EmptySuiteThrows) {
    TestSuite suite;

    EXPECT_THROW(
        suite.getTestCase(0),
        std::out_of_range
    );
}


TEST(TestSuite,ConstSuiteCanBeRead){
    TestCase testcase{
        "input",
        "output"
    };


    TestSuite suite;
    suite.addTestCase(testcase);


    const TestSuite& testsuite = suite;

    const TestCase& result = testsuite.getTestCase(0);

    EXPECT_EQ(result.input, "input");
    EXPECT_EQ(result.expected_output, "output");

}