#pragma once 

#include "testcase/test_case.hpp"
#include <vector>

class TestSuite{
    public:
        void addTestCase(const TestCase& test_case);
        const TestCase getTestCase(size_t  index)const;
        size_t size()const;
    private:
        std::vector<TestCase> test_cases;
};