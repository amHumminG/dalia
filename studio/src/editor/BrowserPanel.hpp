#pragma once
#include "editor/Panel.hpp"
#include "project/Project.hpp"
#include "editor/SelectionContext.hpp"

namespace dalia::studio {

	class BrowserPanel : public Panel {
	public:
		BrowserPanel(Project& project, SelectionContext& selection);
		~BrowserPanel() override;

	protected:
		void Render() override;

	private:
		Project& m_project;
		SelectionContext& m_selectionContext;
	};
}
