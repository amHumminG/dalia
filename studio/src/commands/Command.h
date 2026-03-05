#pragma once
#include <string>

namespace dalia::studio {

    class Command {
    public:
        Command() = default;
        virtual ~Command() = default;

        virtual void Execute() = 0;
        virtual void Unexecute() = 0;
        virtual std::string GetName() const = 0;
    };
}