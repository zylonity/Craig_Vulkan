#pragma once


namespace Craig {

	template<typename Context, typename StateId>
	class StateMachine;

	// one state in a StateMachine
	// Context is whatever the machine hands every state
	// StateId is an enum class naming them so nothing needs the objects themselves
	template<typename Context, typename StateId>
	class State {

	public:
		virtual ~State() = default;

		// shown in the log and the editor
		virtual const char* getName() const = 0;

		// pFrom/pTo = the other side of the transition, nullptr when the machine starts/stops
		virtual void onEnter(Context& context, const State* pFrom) {}
		virtual void onExit(Context& context, const State* pTo) {}

		// something got pushed on top / popped back off
		// covered states don't get update()
		virtual void onCovered(Context& context) {}
		virtual void onUncovered(Context& context) {}

		// only the top of the stack gets this
		virtual void update(Context& context, const float& deltaTime) {}

		StateId getId() const { return m_id; }

	protected:
		// queued, same as asking the machine
		void requestChange(StateId id) { mp_machine->requestChange(id); }
		void requestPush(StateId id) { mp_machine->requestPush(id); }
		void requestPop() { mp_machine->requestPop(); }

	private:
		// set by the machine when the state's added
		friend class StateMachine<Context, StateId>;
		StateId m_id{};
		StateMachine<Context, StateId>* mp_machine = nullptr;
	};

}
