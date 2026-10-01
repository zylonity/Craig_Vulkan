#pragma once
#include "Craig_State.hpp"
#include "Craig/Craig_Logger.hpp"

#include <cassert>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Craig {

	// generic state machine with a stack, used for the engine modes and the game's own flow
	// requests get queued and only happen in applyRequests()
	// so you can't delete a scene out from under something thats still looping over it
	// changes/pushes have to be in the transition table or they get refused
	template<typename Context, typename StateId>
	class StateMachine {

	public:
		using StateType = State<Context, StateId>;

		explicit StateMachine(std::string name) : m_name(std::move(name)) {}

		// states keep a pointer back to the machine, so it can't move
		StateMachine(const StateMachine&) = delete;
		StateMachine& operator=(const StateMachine&) = delete;

		// the machine owns the context
		// fill in its pointers before start()
		Context& getContext() { return m_context; }
		const Context& getContext() const { return m_context; }

		void addState(StateId id, std::unique_ptr<StateType> pState)
		{
			assert(pState != nullptr && "Adding a null state, that's not gonna go well");
			assert(m_states.find(id) == m_states.end() && "Already got a state with that id");
			pState->m_id = id;
			pState->mp_machine = this;
			m_states[id] = std::move(pState);
		}

		// one way only, add the reverse too if you want it
		void allowTransition(StateId from, StateId to) { m_allowedTransitions.insert({ from, to }); }

		// enters the first state straight away
		void start(StateId id)
		{
			assert(mv_stack.empty() && "Machine's already running");
			StateType* pState = getState(id);
			Craig::Logger::state().info("[{}] Starting in {}", m_name, pState->getName());
			mv_stack.push_back(pState);
			pState->onEnter(m_context, nullptr);
		}

		// exits everything top down, binning anything still queued
		void stop()
		{
			mv_requests.clear();
			while (!mv_stack.empty())
			{
				StateType* pState = mv_stack.back();
				mv_stack.pop_back();
				Craig::Logger::state().info("[{}] Stopping {}", m_name, pState->getName());
				pState->onExit(m_context, nullptr);
			}
		}

		bool isRunning() const { return !mv_stack.empty(); }

		// swaps the top of the stack for another state
		void requestChange(StateId id) { mv_requests.push_back({ RequestType::Change, id }); }
		// puts a state on top, the one underneath stops updating but stays alive
		void requestPush(StateId id) { mv_requests.push_back({ RequestType::Push, id }); }
		// back to whatever's underneath, as long as it's not the last one
		void requestPop() { mv_requests.push_back({ RequestType::Pop, StateId{} }); }

		// for greying out buttons, only checks the current state
		bool canChangeTo(StateId id) const { return !mv_stack.empty() && isAllowed(mv_stack.back()->getId(), id); }

		// once a frame from one spot, before anything uses the state
		void applyRequests()
		{
			// swapped out first so requests from onEnter/onExit wait for next frame instead of looping forever
			std::vector<Request> requests;
			requests.swap(mv_requests);

			for (const Request& request : requests)
			{
				switch (request.type)
				{
				case RequestType::Change: applyChange(request.id); break;
				case RequestType::Push: applyPush(request.id); break;
				case RequestType::Pop: applyPop(); break;
				}
			}
		}

		void update(const float& deltaTime)
		{
			if (!mv_stack.empty())
			{
				mv_stack.back()->update(m_context, deltaTime);
			}
		}

		StateType* getCurrent() const { return mv_stack.empty() ? nullptr : mv_stack.back(); }
		bool isCurrent(StateId id) const { return !mv_stack.empty() && mv_stack.back()->getId() == id; }
		// bottom first, current state is the last one
		const std::vector<StateType*>& getStack() const { return mv_stack; }
		const std::string& getName() const { return m_name; }

	private:
		enum class RequestType { Change, Push, Pop };
		struct Request {
			RequestType type;
			StateId id;
		};

		StateType* getState(StateId id) const
		{
			const auto it = m_states.find(id);
			assert(it != m_states.end() && "Asked for a state that was never added");
			return it->second.get();
		}

		bool isAllowed(StateId from, StateId to) const { return m_allowedTransitions.count({ from, to }) > 0; }

		void applyChange(StateId id)
		{
			assert(!mv_stack.empty() && "Changing state before start()");
			StateType* pFrom = mv_stack.back();
			StateType* pTo = getState(id);
			if (!isAllowed(pFrom->getId(), id))
			{
				Craig::Logger::state().warn("[{}] Can't go from {} to {}, not in the transition table", m_name, pFrom->getName(), pTo->getName());
				return;
			}

			Craig::Logger::state().info("[{}] {} -> {}", m_name, pFrom->getName(), pTo->getName());
			pFrom->onExit(m_context, pTo);
			mv_stack.back() = pTo;
			pTo->onEnter(m_context, pFrom);
		}

		void applyPush(StateId id)
		{
			assert(!mv_stack.empty() && "Pushing a state before start()");
			StateType* pBelow = mv_stack.back();
			StateType* pTo = getState(id);
			if (!isAllowed(pBelow->getId(), id))
			{
				Craig::Logger::state().warn("[{}] Can't push {} on top of {}, not in the transition table", m_name, pTo->getName(), pBelow->getName());
				return;
			}

			Craig::Logger::state().info("[{}] Pushed {} on top of {}", m_name, pTo->getName(), pBelow->getName());
			pBelow->onCovered(m_context);
			mv_stack.push_back(pTo);
			pTo->onEnter(m_context, pBelow);
		}

		void applyPop()
		{
			if (mv_stack.size() < 2)
			{
				Craig::Logger::state().warn("[{}] Can't pop {}, it's the only state left", m_name, mv_stack.empty() ? "nothing" : mv_stack.back()->getName());
				return;
			}

			StateType* pFrom = mv_stack.back();
			StateType* pBelow = mv_stack[mv_stack.size() - 2];

			Craig::Logger::state().info("[{}] Popped {}, back to {}", m_name, pFrom->getName(), pBelow->getName());
			pFrom->onExit(m_context, pBelow);
			mv_stack.pop_back();
			pBelow->onUncovered(m_context);
		}

		std::string m_name; // goes in front of every log line
		Context m_context{};

		std::unordered_map<StateId, std::unique_ptr<StateType>> m_states;
		std::set<std::pair<StateId, StateId>> m_allowedTransitions;

		std::vector<StateType*> mv_stack; // not owned, they live in m_states
		std::vector<Request> mv_requests;
	};

}
