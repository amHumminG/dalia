#pragma once
#include "project/Project.hpp"
#include "editor/Panel.hpp"
#include "editor/SelectionContext.hpp"
#include "commands/CommandStack.hpp"

#include "dalia/dalia.hpp"

#include <string>
#include <vector>
#include <memory>

namespace dalia::studio {

	class Application {
	public:
		Application(int width, int height, const std::string& title);
		~Application();

		void Run();

	private:
		void SetupTheme();
		void InitEngine();
		void Update();
		void Render();
		void RenderUI();

		std::unique_ptr<Engine> m_engine;
		SelectionContext m_selectionContext;
		std::unique_ptr<Project> m_project; // Model
		std::vector<std::unique_ptr<Panel>> m_panels; // View
		std::unique_ptr<CommandStack> m_commands; // Controller
	};
}
