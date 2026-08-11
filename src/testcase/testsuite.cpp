#include "testcase/testsuite.hpp"

#include <stdexcept>
#include <cstddef>

const TestCase& TestSuite::getTestCase(std::size_t index)const{
    if(index >= test_cases.size()){
        throw std::out_of_range("invalid index");
    }
    return  test_cases[index];
}

std::size_t TestSuite::size()const{
    return  test_cases.size();
}


void  TestSuite::addTestCase(const TestCase& test_case){
    test_cases.push_back(test_case);
} 

