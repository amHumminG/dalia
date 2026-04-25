#pragma once

#include "dalia.h"

class Panel {
public:
	virtual ~Panel() = default;
	virtual void Draw(dalia::Engine& engine) = 0; // Do we need this?
	bool isOpen = true;
};