#include "Craig_SoundPathing.hpp"
#include "Craig_SphericalHarmonics.hpp"
#include "Craig_AcousticMaterial.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>
#include <queue>

namespace {
	constexpr float kPi = 3.14159265358979323846f;

	glm::vec3 unitVector(const glm::vec3& v) {
		const float length = glm::length(v);
		return length > 0.0f ? v / length : v;
	}

	//vector.h angleBetween, atan2 of the perpendicular and parallel parts
	float angleBetween(const glm::vec3& from, const glm::vec3& to) {
		const glm::vec3 parallel = glm::dot(to, from) * from;
		const glm::vec3 perpendicular = to - parallel;
		float angle = std::atan2(glm::length(perpendicular), glm::length(parallel));
		if (angle < 0.0f) angle += 2.0f * kPi;
		return angle;
	}

	//distance_attenuation.cpp default model, nothing gets louder inside 1 m
	float distanceAttenuation(float distance) { return 1.0f / std::max(distance, 1.0f); }

	//eq_effect.cpp normalizeGains
	void normalizeGains(std::array<float, 3>& gains, float& overallGain) {
		constexpr float kMaxEQGain = 0.0625f;
		float maxGain = 0.0f;
		for (float gain : gains) maxGain = std::max(maxGain, gain);
		if (maxGain < std::numeric_limits<float>::min()) {
			overallGain = 0.0f;
			gains = {1.0f, 1.0f, 1.0f};
			return;
		}
		for (float& gain : gains) gain = std::max(gain / maxGain, kMaxEQGain);
		overallGain *= maxGain;
	}

	//path_finder.cpp's priority queue, cheapest on top
	struct QueueEntry {
		uint32_t node;
		float cost;
		bool operator<(const QueueEntry& other) const { return cost > other.cost; }
	};
}

float Craig::utdDeviation(float angle, uint32_t band) {
	using complex = std::complex<float>;
	const auto cotf = [](float theta) { return 1.0f / std::tan(theta); };
	const auto plusN = [](float n, float x) { return x <= kPi * (n - 1.0f) ? 0.0f : 1.0f; };
	const auto minusN = [](float n, float x) {
		if (x < kPi * (1.0f - n)) return -1.0f;
		if (kPi * (1.0f - n) <= x && x <= kPi * (1.0f + n)) return 0.0f;
		return 1.0f;
	};
	const auto a = [](float n, float beta, float N) {
		const float cosine = std::cos(kPi * n * N - 0.5f * beta);
		return 2.0f * cosine * cosine;
	};
	const auto F = [](float x) {
		const complex e = std::polar(1.0f, 0.25f * kPi * std::sqrt(x / (x + 1.4f)));
		if (x < 0.8f) return std::sqrt(kPi * x) * (1.0f - std::sqrt(x) / (0.7f * std::sqrt(x) + 1.2f)) * e;
		return (1.0f - 0.8f / ((x + 1.25f) * (x + 1.25f))) * e;
	};
	const auto sign = [](float x) { return x > 0.0f ? 1.0f : -1.0f; };

	//A wedge, sound coming in along one face and bending by the angle round the other
	const float n = 2.0f;
	const float alphaI = 0.0f;
	const float alphaD = alphaI + kPi + angle;
	const float L = 0.05f;
	//Middle of the band, (0 + 800) / 2, (800 + 8000) / 2 and (8000 + 22000) / 2 like bands.cpp
	constexpr std::array<float, 3> centres = {400.0f, 4400.0f, 15000.0f};
	const float k = 2.0f * kPi / (kSpeedOfSound / centres[band]);

	const complex e = std::polar(1.0f, -0.25f * kPi);
	const complex D0 = e / (2.0f * n * std::sqrt(2.0f * kPi * k));
	const float betas[4] = {alphaD - alphaI, alphaD - alphaI, alphaD + alphaI, alphaD + alphaI};
	const float t[4] = {cotf((kPi + betas[0]) / (2.0f * n)), cotf((kPi - betas[1]) / (2.0f * n)),
		cotf((kPi + betas[2]) / (2.0f * n)), cotf((kPi - betas[3]) / (2.0f * n))};
	const float N[4] = {plusN(n, betas[0]), minusN(n, betas[1]), plusN(n, betas[2]), minusN(n, betas[3])};
	complex D[4];
	for (int i = 0; i < 4; ++i) {
		D[i] = t[i] * F(k * L * a(n, betas[i], N[i]));
		if (!std::isfinite(t[i])) {
			//Right on a shadow boundary the cotangent blows up, upstream swaps in the limit
			const float epsilon = i % 2 == 0 ? betas[i] - 2.0f * kPi * n * N[i] + kPi : -(betas[i] - 2.0f * kPi * n * N[i] - kPi);
			D[i] = n * e * (std::sqrt(2.0f * kPi * k * L) * sign(epsilon) - 2.0f * k * L * epsilon * e);
		}
	}
	return std::abs(D0 * (D[0] + D[1] + D[2] + D[3]));
}

std::vector<Craig::SoundPathing::Pair> Craig::SoundPathing::candidatePairs(const std::vector<ProbeSphere>& probes, float visRange) {
	std::vector<Pair> pairs;
	const glm::vec3 down(0.0f, -1.0f, 0.0f);
	for (uint32_t i = 0; i < probes.size(); ++i) {
		for (uint32_t j = 0; j < i; ++j) {
			//Height difference doesn't count, so probes stacked over each other on stairs still link up
			glm::vec3 d = probes[i].centre - probes[j].centre;
			d -= glm::dot(d, down) * down;
			if (glm::length(d) > visRange) continue;
			pairs.push_back({i, j});
		}
	}
	return pairs;
}

void Craig::SoundPathing::clear() {
	mv_probes.clear();
	mv_adjacent.clear();
	mv_parents.clear();
	m_edges = 0;
}

void Craig::SoundPathing::build(const std::vector<ProbeSphere>& probes, const std::vector<Pair>& pairs, const std::vector<uint8_t>& visible,
	float pathRange) {
	clear();
	const uint32_t count = static_cast<uint32_t>(probes.size());
	if (count == 0 || pairs.size() != visible.size()) return;
	mv_probes = probes;

	//ProbeVisibilityGraph: each row gets its j < i edges in order, then the reverse ones get added after
	//Kept in the same order as upstream so equal length paths break the same way
	mv_adjacent.assign(count, {});
	for (size_t p = 0; p < pairs.size(); ++p) {
		if (!visible[p]) continue;
		const auto [i, j] = pairs[p];
		mv_adjacent[i].push_back({j, glm::length(probes[i].centre - probes[j].centre)});
		m_edges++;
	}
	for (uint32_t i = 0; i < count; ++i) {
		const size_t original = mv_adjacent[i].size();
		for (size_t e = 0; e < original; ++e) {
			if (mv_adjacent[i][e].first < i) mv_adjacent[mv_adjacent[i][e].first].push_back({i, mv_adjacent[i][e].second});
		}
	}

	//PathFinder::findAllShortestPaths from every probe
	mv_parents.assign(static_cast<size_t>(count) * count, -1);
	std::vector<float> costs(count);
	for (uint32_t start = 0; start < count; ++start) {
		int32_t* parents = &mv_parents[static_cast<size_t>(start) * count];
		std::fill(costs.begin(), costs.end(), std::numeric_limits<float>::infinity());
		costs[start] = 0.0f;
		std::priority_queue<QueueEntry> queue;
		queue.push({start, 0.0f});
		while (!queue.empty()) {
			const uint32_t u = queue.top().node;
			queue.pop();
			for (const auto& [v, cost] : mv_adjacent[u]) {
				const float newCost = costs[u] + cost;
				if (newCost > pathRange) continue;
				if (newCost < costs[v]) {
					costs[v] = newCost;
					parents[v] = static_cast<int32_t>(u);
					queue.push({v, newCost});
				}
			}
		}
	}
}

bool Craig::SoundPathing::hasPath(uint32_t start, uint32_t end) const {
	return mv_parents[static_cast<size_t>(start) * mv_probes.size() + end] >= 0;
}

std::vector<int> Craig::SoundPathing::nodes(uint32_t start, uint32_t end) const {
	const int32_t* parents = &mv_parents[static_cast<size_t>(start) * mv_probes.size()];
	std::vector<int> result;
	for (int parent = parents[end]; parent >= 0 && parent != static_cast<int>(start); parent = parents[parent]) result.push_back(parent);
	std::reverse(result.begin(), result.end());
	return result;
}

//SoundPath(ProbePath) from path_data.cpp
Craig::SoundPathing::SoundPath Craig::SoundPathing::makePath(uint32_t start, uint32_t end) const {
	SoundPath path;
	if (!hasPath(start, end)) return path;
	const std::vector<int> between = nodes(start, end);
	if (between.empty()) {
		path.direct = true;
		return path;
	}
	path.firstProbe = between.front();
	path.lastProbe = between.back();
	if (between.size() >= 2) {
		path.probeAfterFirst = between[1];
		path.probeBeforeLast = between[between.size() - 2];
	}
	for (size_t i = 1; i < between.size(); ++i) {
		path.distanceInternal += glm::length(mv_probes[between[i]].centre - mv_probes[between[i - 1]].centre);
	}
	for (size_t i = 1; i + 1 < between.size(); ++i) {
		const glm::vec3 previous = unitVector(mv_probes[between[i]].centre - mv_probes[between[i - 1]].centre);
		const glm::vec3 next = unitVector(mv_probes[between[i + 1]].centre - mv_probes[between[i]].centre);
		path.deviationInternal += angleBetween(previous, next);
	}
	return path;
}

//BakedPathData::lookupShortestPath, the higher index's search gets used both ways round
Craig::SoundPathing::SoundPath Craig::SoundPathing::lookupShortestPath(uint32_t start, uint32_t end) const {
	if (start < end) {
		SoundPath path = makePath(end, start);
		std::swap(path.firstProbe, path.lastProbe);
		std::swap(path.probeAfterFirst, path.probeBeforeLast);
		return path;
	}
	return makePath(start, end);
}

float Craig::SoundPathing::distance(const SoundPath& path, uint32_t start, uint32_t end) const {
	if (path.direct) return glm::length(mv_probes[end].centre - mv_probes[start].centre);
	return path.distanceInternal + glm::length(mv_probes[path.firstProbe].centre - mv_probes[start].centre) +
		glm::length(mv_probes[end].centre - mv_probes[path.lastProbe].centre);
}

float Craig::SoundPathing::deviation(const SoundPath& path, uint32_t start, uint32_t end) const {
	float result = path.deviationInternal;
	if (path.direct) return result;
	const auto turn = [&](int a, int b, int c) {
		return angleBetween(unitVector(mv_probes[b].centre - mv_probes[a].centre), unitVector(mv_probes[c].centre - mv_probes[b].centre));
	};
	if (path.probeAfterFirst < 0 && path.probeBeforeLast < 0) {
		result += turn(static_cast<int>(start), path.firstProbe, static_cast<int>(end));
	}
	else {
		if (path.probeAfterFirst >= 0) result += turn(static_cast<int>(start), path.firstProbe, path.probeAfterFirst);
		if (path.probeBeforeLast >= 0) result += turn(path.probeBeforeLast, path.lastProbe, static_cast<int>(end));
	}
	return result;
}

//The path's total length, but coming in from the direction of the last probe before the listener's
glm::vec3 Craig::SoundPathing::toVirtualSource(const SoundPath& path, uint32_t start, uint32_t end) const {
	if (path.direct) return mv_probes[start].centre;
	const glm::vec3 direction = unitVector(mv_probes[path.lastProbe].centre - mv_probes[end].centre);
	return mv_probes[end].centre + distance(path, start, end) * direction;
}

uint32_t Craig::SoundPathing::influencingProbes(const glm::vec3& point, std::array<uint32_t, kMaxNeighbours>& probes) const {
	std::vector<std::pair<float, uint32_t>> inside;
	for (uint32_t i = 0; i < mv_probes.size(); ++i) {
		const float d = glm::length(point - mv_probes[i].centre);
		if (d <= mv_probes[i].radius) inside.push_back({d, i});
	}
	std::sort(inside.begin(), inside.end());
	const uint32_t count = std::min<uint32_t>(static_cast<uint32_t>(inside.size()), kMaxNeighbours);
	for (uint32_t i = 0; i < count; ++i) probes[i] = inside[i].second;
	return count;
}

Craig::PathingResult Craig::SoundPathing::findPaths(const Query& query) const {
	struct Found {
		SoundPath path;
		float weight;
		int start;
		int end;
	};
	std::vector<Found> paths;
	PathingResult result;
	result.order = std::min(query.order, kMaxReflectionOrder);

	if (query.directOccluded) {
		//ProbeNeighborhood::calcWeights, inverse distance over the probes the listener can see
		std::vector<std::pair<uint32_t, float>> listeners;
		float total = 0.0f;
		for (uint32_t i = 0; i < query.listenerCount; ++i) {
			if (query.listenerOccluded[i]) continue;
			const float weight = 1.0f / (glm::length(query.listener - mv_probes[query.listenerProbes[i]].centre) + 1e-4f);
			listeners.push_back({query.listenerProbes[i], weight});
			total += weight;
		}
		//Only the nearest source probe the source can see, sEnablePathsFromAllSourceProbes is off upstream
		int sourceProbe = -1;
		float nearest = std::numeric_limits<float>::infinity();
		for (uint32_t i = 0; i < query.sourceCount; ++i) {
			if (query.sourceOccluded[i]) continue;
			const glm::vec3 offset = query.source - mv_probes[query.sourceProbes[i]].centre;
			const float d = glm::dot(offset, offset);
			if (d < nearest) {
				nearest = d;
				sourceProbe = static_cast<int>(query.sourceProbes[i]);
			}
		}
		//Upstream leaves the last frame's numbers in place here, silence is the honest version
		if (listeners.empty() || sourceProbe < 0) return result;
		for (const auto& [probe, weight] : listeners) {
			const SoundPath path = lookupShortestPath(static_cast<uint32_t>(sourceProbe), probe);
			if (path.isValid()) paths.push_back({path, weight / total, sourceProbe, static_cast<int>(probe)});
		}
	}
	else {
		SoundPath direct;
		direct.direct = true;
		paths.push_back({direct, 1.0f, -1, -1});
	}
	if (paths.empty()) return result;

	//calcAmbisonicsCoeffsForPaths
	float coefficients[SphericalHarmonics::kMaxCoefficients];
	const uint32_t channels = ReflectionField::channelsFor(result.order);
	for (const Found& found : paths) {
		glm::vec3 virtualSource;
		float d;
		if (found.start >= 0 && found.end >= 0) {
			virtualSource = toVirtualSource(found.path, found.start, found.end);
			d = glm::length(virtualSource - mv_probes[found.end].centre);
		}
		else {
			virtualSource = query.source;
			d = glm::length(virtualSource - query.listener);
		}
		const float gain = found.weight * distanceAttenuation(d);
		const glm::vec3 direction = unitVector(virtualSource - query.listener);
		SphericalHarmonics::evaluate(result.order, direction.x, direction.y, direction.z, coefficients);
		for (uint32_t c = 0; c < channels; ++c) result.sh[c] += gain * coefficients[c];
	}

	//calcEQForPaths, how much each band loses bending round the corners compared to going straight
	result.eq = {0.0f, 0.0f, 0.0f};
	for (const Found& found : paths) {
		if (found.start >= 0 && found.end >= 0) {
			const float angle = std::max(1e-8f, deviation(found.path, found.start, found.end));
			std::array<float, 3> term{};
			for (uint32_t band = 0; band < 3; ++band) term[band] = utdDeviation(angle, band) / utdDeviation(1e-8f, band);
			float overall = 1.0f;
			normalizeGains(term, overall);
			for (uint32_t band = 0; band < 3; ++band) result.eq[band] += found.weight * overall * term[band];
		}
		else {
			for (float& gain : result.eq) gain += found.weight;
		}
	}

	//Route for the overlay: source, its probe, the probes between, the listener's probe, then the listener
	for (const Found& found : paths) {
		if (found.start < 0) {
			result.segments.push_back({query.source.x, query.source.y, query.source.z});
			result.segments.push_back({query.listener.x, query.listener.y, query.listener.z});
			continue;
		}
		std::vector<glm::vec3> route = {query.source, mv_probes[found.start].centre};
		const uint32_t high = std::max<uint32_t>(found.start, found.end), low = std::min<uint32_t>(found.start, found.end);
		std::vector<int> between = nodes(high, low);
		if (found.start < found.end) std::reverse(between.begin(), between.end());
		for (int node : between) route.push_back(mv_probes[node].centre);
		route.push_back(mv_probes[found.end].centre);
		route.push_back(query.listener);
		for (size_t i = 1; i < route.size(); ++i) {
			result.segments.push_back({route[i - 1].x, route[i - 1].y, route[i - 1].z});
			result.segments.push_back({route[i].x, route[i].y, route[i].z});
		}
	}
	result.valid = true;
	return result;
}
