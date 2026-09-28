#pragma once
#include "Craig/Craig_Constants.hpp"
#include "Craig_Component.hpp"

#include <string>

#include "../External/Imgui/imgui.h"
#include "../External/Imgui/imfilebrowser.h"


namespace Craig {

	namespace Components
	{
		// makes the game object draw a model
		// not Craig::Model (the loaded data in the ResourceManager), this just says which one to use
		class Model : public Component {

		public:
			CraigError init() override;
			CraigError update() override;
			CraigError terminate() override;

			CraigError loadFromJson(const nlohmann::json& json) override;
			void saveToJson(nlohmann::json& json) const override;
			void displayImGuiAttributes() override;

			const char* getTypeName() const override { return "Model"; }
			const char* getJsonKey() const override { return "model"; }

			// Loads the model if it isn't already and tells the scene its geometry needs rebuilding
			CraigError setModelPath(const std::string& modelPath);
			const std::string& getModelPath() const { return m_modelPath; }
			bool hasModel() const { return !m_modelPath.empty(); }

		private:
			void markSceneGeometryDirty();

			std::string m_modelPath;

			// editor only
			std::string m_modelPathError;
			ImGui::FileBrowser m_modelBrowser;
		};
	}

}
