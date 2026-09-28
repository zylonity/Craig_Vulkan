#pragma once
#include "Craig/Craig_Constants.hpp"
#include "../External/json.hpp"


namespace Craig {
	class GameObject;

	namespace Components
	{
		// base class for anything you can stick on a game object
		// one of each component type per game object
		class Component {

		public:
			virtual ~Component() = default;

			virtual CraigError init() { return CRAIG_SUCCESS; }
			virtual CraigError update() { return CRAIG_SUCCESS; }
			virtual CraigError terminate() { return CRAIG_SUCCESS; }

			// reads the component's settings out of its block in the scene json
			virtual CraigError loadFromJson(const nlohmann::json& json) { return CRAIG_SUCCESS; }
			// writes them back out in the same format loadFromJson reads
			virtual void saveToJson(nlohmann::json& json) const {}

			// Draws the component's editable properties in the editor
			virtual void displayImGuiAttributes() {}

			// shown in the editor
			virtual const char* getTypeName() const = 0;
			// the component's key under "components" in the scene json
			virtual const char* getJsonKey() const = 0;

			GameObject* getOwner() const { return mp_owner; }
			void setOwner(GameObject* pOwner) { mp_owner = pOwner; }

		protected:
			GameObject* mp_owner = nullptr;
		};
	}

}
