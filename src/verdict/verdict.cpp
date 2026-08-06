
#include "common/verdict.hpp"



std::string to_string(Verdict verdict){
    switch(verdict)
    {
        case Verdict::Accepted:
            return "Accepted";

        case Verdict::WrongAnswer:
            return "Wrong Answer";

        case Verdict::RuntimeError:
            return "Runtime Error";
    }
    return "Unkown Verdict";
}

std::ostream& operator<<(std::ostream& os, Verdict verdict)
{
    return os<< to_string(verdict);
}