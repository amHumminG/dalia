#pragma once

class Panel {
public:
	virtual ~Panel() = default;
	virtual void Draw() = 0; // Do we need this?
	bool isOpen = true;
};