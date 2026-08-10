#include "testcase/testsuite.hpp"

#include <stdexcept>

const TestCase& TestSuite::getTestCase(size_t index)const{
    if(index >= test_case.size()){
        throw std::out_of_range("invalid index");
    }
    return  test_case[index];
}

