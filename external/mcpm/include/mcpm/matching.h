#pragma once

#include <mcpm/graph.h>
#include <mcpm/binary_heap.h>
#include <list>
#include <vector>

namespace mcpm {

	class Matching {

		public:
			//Parametric constructor receives a graph instance
			Matching(const Graph & G):
				G(G),
				outer(2*G.GetNumVertices()),
				deep(2*G.GetNumVertices()),
				shallow(2*G.GetNumVertices()),
				tip(2*G.GetNumVertices()),
				active(2*G.GetNumVertices()),
				type(2*G.GetNumVertices()),
				forest(2*G.GetNumVertices()),
				root(2*G.GetNumVertices()),
				blocked(2*G.GetNumVertices()),
				dual(2*G.GetNumVertices()),
				slack(G.GetNumEdges()),
				mate(2*G.GetNumVertices()),
				m(G.GetNumEdges()),
				n(G.GetNumVertices()),
				visited(2*G.GetNumVertices()) { }

			//Solves the minimum cost perfect matching problem
			//Receives the a vector whose position i has the cost of the edge with index i
			//If the graph doest not have a perfect matching, a const char * exception will be raised
			//Returns a pair
			//the first element of the pair is a list of the indices of the edges in the matching
			//the second is the cost of the matching
			std::pair< std::list<int>, double > SolveMinimumCostPerfectMatching(const std::vector<double> & cost) {
				SolveMaximumMatching();
				if(!perfect)
					throw "Error: The graph does not have a perfect matching";

				Clear();

				//Initialize slacks (reduced costs for the edges)
				slack = cost;

				PositiveCosts();

				//If the matching on the compressed graph is perfect, we are done
				perfect = false;
				while(not perfect)
				{
					//Run an heuristic maximum matching algorithm
					Heuristic();
					//Grow a hungarian forest
					Grow();
					UpdateDualCosts();
					//Set up the algorithm for a new grow step
					Reset();
				}

				std::list<int> matching = RetrieveMatching();

				double obj = 0;
				for(std::list<int>::iterator it = matching.begin(); it != matching.end(); it++)
					obj += cost[*it];

				double dualObj = 0;
				for(int i = 0; i < 2*n; i++)
				{
					if(i < n) dualObj += dual[i];
					else if(blocked[i]) dualObj += dual[i];
				}

				return std::pair< std::list<int>, double >(matching, obj);
			}

			//Solves the maximum cardinality matching problem
			//Returns a list with the indices of the edges in the matching
			std::list<int> SolveMaximumMatching() {
				Clear();
				Grow();
				return RetrieveMatching();
			}

		private:
			static constexpr int EVEN = 2;
			static constexpr int ODD = 1;
			static constexpr int UNLABELED = 0;

			//Grows an alternating forest
			void Grow() {
				Reset();

				//All unmatched vertices will be roots in a forest that will be grown
				//The forest is grown by extending a unmatched vertex w through a matched edge u-v in a BFS fashion
				while(!forestList.empty())
				{
					int w = outer[forestList.front()];
					forestList.pop_front();

					//w might be a blossom
					//we have to explore all the connections from vertices inside the blossom to other vertices
					for(std::list<int>::iterator it = deep[w].begin(); it != deep[w].end(); it++)
					{
						int u = *it;

						int cont = false;
						for(std::list<int>::const_iterator jt = G.AdjList(u).begin(); jt != G.AdjList(u).end(); jt++)
						{
							int v = *jt;

							if(IsEdgeBlocked(u, v)) continue;

							//u is even and v is odd
							if(type[outer[v]] == ODD) continue;

							//if v is unlabeled
							if(type[outer[v]] != EVEN)
							{
								//We grow the alternating forest
								int vm = mate[outer[v]];

								forest[outer[v]] = u;
								type[outer[v]] = ODD;
								root[outer[v]] = root[outer[u]];
								forest[outer[vm]] = v;
								type[outer[vm]] = EVEN;
								root[outer[vm]] = root[outer[u]];

								if(!visited[outer[vm]])
								{
									forestList.push_back(vm);
									visited[outer[vm]] = true;
								}
							}
							//If v is even and u and v are on different trees
							//we found an augmenting path
							else if(root[outer[v]] != root[outer[u]])
							{
								Augment(u,v);
								Reset();

								cont = true;
								break;
							}
							//If u and v are even and on the same tree
							//we found a blossom
							else if(outer[u] != outer[v])
							{
								int b = Blossom(u,v);

								forestList.push_front(b);
								visited[b] = true;

								cont = true;
								break;
							}
						}
						if(cont) break;
					}
				}

				//Check whether the matching is perfect
				perfect = true;
				for(int i = 0; i < n; i++)
					if(mate[outer[i]] == -1)
						perfect = false;
			}

			//Expands a blossom u
			//If expandBlocked is true, the blossom will be expanded even if it is blocked
			void Expand(int u, bool expandBlocked = false) {
				int v = outer[mate[u]];

				int index = m;
				int p = -1, q = -1;
				//Find the regular edge {p,q} of minimum index connecting u and its mate
				//We use the minimum index to grant that the two possible blossoms u and v will use the same edge for a mate
				for(std::list<int>::iterator it = deep[u].begin(); it != deep[u].end(); it++) {
					int di = *it;
					for(std::list<int>::iterator jt = deep[v].begin(); jt != deep[v].end(); jt++) {
						int dj = *jt;
						if(IsAdjacent(di, dj) and G.GetEdgeIndex(di, dj) < index) {
							index = G.GetEdgeIndex(di, dj);
							p = di;
							q = dj;
						}
					}
				}

				mate[u] = q;
				mate[v] = p;
				//If u is a regular vertex, we are done
				if(u < n or (blocked[u] and not expandBlocked)) return;

				bool found = false;
				//Find the position t of the new tip of the blossom
				for(std::list<int>::iterator it = shallow[u].begin(); it != shallow[u].end() and not found; ) {
					int si = *it;
					for(std::list<int>::iterator jt = deep[si].begin(); jt != deep[si].end() and not found; jt++) {
						if(*jt == p )
							found = true;
					}
					it++;
					if(not found) {
						shallow[u].push_back(si);
						shallow[u].pop_front();
					}
				}

				std::list<int>::iterator it = shallow[u].begin();
				//Adjust the mate of the tip
				mate[*it] = mate[u];
				it++;
				//
				//Now we go through the odd circuit adjusting the new mates
				while(it != shallow[u].end()) {
					std::list<int>::iterator itnext = it;
					itnext++;
					mate[*it] = *itnext;
					mate[*itnext] = *it;
					itnext++;
					it = itnext;
				}

				//We update the sets blossom, shallow, and outer since this blossom is being deactivated
				for(std::list<int>::iterator it = shallow[u].begin(); it != shallow[u].end(); it++) {
					int s = *it;
					outer[s] = s;
					for(std::list<int>::iterator jt = deep[s].begin(); jt != deep[s].end(); jt++)
						outer[*jt] = s;
				}
				active[u] = false;
				AddFreeBlossomIndex(u);

				//Expand the vertices in the blossom
				for(std::list<int>::iterator it = shallow[u].begin(); it != shallow[u].end(); it++)
					Expand(*it, expandBlocked);

			}

			//Augments the matching using the path from u to v in the alternating forest
			//Augment the path root[u], ..., u, v, ..., root[v]
			void Augment(int u, int v) {
				//We go from u and v to its respective roots, alternating the matching
				int p = outer[u];
				int q = outer[v];
				int outv = q;
				int fp = forest[p];
				mate[p] = q;
				mate[q] = p;
				Expand(p);
				Expand(q);
				while(fp != -1) {
					q = outer[forest[p]];
					p = outer[forest[q]];
					fp = forest[p];

					mate[p] = q;
					mate[q] = p;
					Expand(p);
					Expand(q);
				}

				p = outv;
				fp = forest[p];
				while(fp != -1) {
					q = outer[forest[p]];
					p = outer[forest[q]];
					fp = forest[p];

					mate[p] = q;
					mate[q] = p;
					Expand(p);
					Expand(q);
				}
			}

			//Resets the alternating forest
			void Reset() {
				for(int i = 0; i < 2*n; i++) {
					forest[i] = -1;
					root[i] = i;

					if(i >= n and active[i] and outer[i] == i)
						DestroyBlossom(i);
				}

				visited.assign(2*n, 0);
				forestList.clear();
				for(int i = 0; i < n; i++) {
					if(mate[outer[i]] == -1) {
						type[outer[i]] = 2;
						if(!visited[outer[i]])
							forestList.push_back(i);
						visited[outer[i]] = true;
					}
					else type[outer[i]] = 0;
				}
			}

			//Creates a blossom where the tip is the first common vertex in the paths from u and v in the hungarian forest
			//Contracts the blossom w, ..., u, v, ..., w, where w is the first vertex that appears in the paths from u and v to their respective roots
			int Blossom(int u, int v) {
				int t = GetFreeBlossomIndex();

				std::vector<bool> isInPath(2*n, false);

				//Find the tip of the blossom
				int u_ = u;
				while(u_ != -1) {
					isInPath[outer[u_]] = true;
					u_ = forest[outer[u_]];
				}

				int v_ = outer[v];
				while(not isInPath[v_])
					v_ = outer[forest[v_]];
				tip[t] = v_;

				//Find the odd circuit, update shallow, outer, blossom and deep
				//First we construct the set shallow (the odd circuit)
				std::list<int> circuit;
				u_ = outer[u];
				circuit.push_front(u_);
				while(u_ != tip[t]) {
					u_ = outer[forest[u_]];
					circuit.push_front(u_);
				}

				shallow[t].clear();
				deep[t].clear();
				for(std::list<int>::iterator it = circuit.begin(); it != circuit.end(); it++) {
					shallow[t].push_back(*it);
				}

				v_ = outer[v];
				while(v_ != tip[t])
				{
					shallow[t].push_back(v_);
					v_ = outer[forest[v_]];
				}

				//Now we construct deep and update outer
				for(std::list<int>::iterator it = shallow[t].begin(); it != shallow[t].end(); it++) {
					u_ = *it;
					outer[u_] = t;
					for(std::list<int>::iterator jt = deep[u_].begin(); jt != deep[u_].end(); jt++) {
						deep[t].push_back(*jt);
						outer[*jt] = t;
					}
				}

				forest[t] = forest[tip[t]];
				type[t] = EVEN;
				root[t] = root[tip[t]];
				active[t] = true;
				outer[t] = t;
				mate[t] = mate[tip[t]];

				return t;
			}

			void UpdateDualCosts() {
				double e1 = 0, e2 = 0, e3 = 0;
				int inite1 = false, inite2 = false, inite3 = false;
				for(int i = 0; i < m; i++) {
					int u = G.GetEdge(i).first,
							v = G.GetEdge(i).second;

					if( (type[outer[u]] == EVEN and type[outer[v]] == UNLABELED) or (type[outer[v]] == EVEN and type[outer[u]] == UNLABELED) ) {
						if(!inite1 or GREATER(e1, slack[i])) {
							e1 = slack[i];
							inite1 = true;
						}
					}
					else if( (outer[u] != outer[v]) and type[outer[u]] == EVEN and type[outer[v]] == EVEN ) {
						if(!inite2 or GREATER(e2, slack[i])) {
							e2 = slack[i];
							inite2 = true;
						}
					}
				}
				for(int i = n; i < 2*n; i++) {
					if(active[i] and i == outer[i] and type[outer[i]] == ODD and (!inite3 or GREATER(e3, dual[i]))) {
						e3 = dual[i];
						inite3 = true;
					}
				}
				double e = 0;
				if(inite1) e = e1;
				else if(inite2) e = e2;
				else if(inite3) e = e3;

				if(GREATER(e, e2/2.0) and inite2)
					e = e2/2.0;
				if(GREATER(e, e3) and inite3)
					e = e3;

				for(int i = 0; i < 2*n; i++) {
					if(i != outer[i]) continue;

					if(active[i] and type[outer[i]] == EVEN)	{
						dual[i] += e;
					}
					else if(active[i] and type[outer[i]] == ODD) {
						dual[i] -= e;
					}
				}

				for(int i = 0; i < m; i++) {
					int u = G.GetEdge(i).first,
							v = G.GetEdge(i).second;

					if(outer[u] != outer[v]) {
						if(type[outer[u]] == EVEN and type[outer[v]] == EVEN)
							slack[i] -= 2.0*e;
						else if(type[outer[u]] == ODD and type[outer[v]] == ODD)
							slack[i] += 2.0*e;
						else if( (type[outer[v]] == UNLABELED and type[outer[u]] == EVEN) or (type[outer[u]] == UNLABELED and type[outer[v]] == EVEN) )
							slack[i] -= e;
						else if( (type[outer[v]] == UNLABELED and type[outer[u]] == ODD) or (type[outer[u]] == UNLABELED and type[outer[v]] == ODD) )
							slack[i] += e;
					}
				}
				for(int i = n; i < 2*n; i++) {
					if(GREATER(dual[i], 0)) {
						blocked[i] = true;
					}
					else if(active[i] and blocked[i]) {
						//The blossom is becoming unblocked
						if(mate[i] == -1) {
							DestroyBlossom(i);
						}
						else {
							blocked[i] = false;
							Expand(i);
						}
					}
				}
			}

			//Resets all data structures
			void Clear() {
				ClearBlossomIndices();

				for(int i = 0; i < 2*n; i++) {
					outer[i] = i;
					deep[i].clear();
					if(i<n)
						deep[i].push_back(i);
					shallow[i].clear();
					if(i < n) active[i] = true;
					else active[i] = false;

					type[i] = 0;
					forest[i] = -1;
					root[i] = i;

					blocked[i] = false;
					dual[i] = 0;
					mate[i] = -1;
					tip[i] = i;
				}
				slack.assign(m, 0);
			}

			//Destroys a blossom recursively
			void DestroyBlossom(int t) {
				if((t < n) or
						(blocked[t] and GREATER(dual[t], 0))) return;

				for(std::list<int>::iterator it = shallow[t].begin(); it != shallow[t].end(); it++) {
					int s = *it;
					outer[s] = s;
					for(std::list<int>::iterator jt = deep[s].begin(); jt != deep[s].end(); jt++)
						outer[*jt] = s;

					DestroyBlossom(s);
				}

				active[t] = false;
				blocked[t] = false;
				AddFreeBlossomIndex(t);
				mate[t] = -1;
			}

			//Uses an heuristic algorithm to find the maximum matching of the graph
			//Vertices will be selected in non-decreasing order of their degree
			//Each time an unmatched vertex is selected, it is matched to its adjacent unmatched vertex of minimum degree
			void Heuristic() {
				std::vector<int> degree(n, 0);
				BinaryHeap B;

				for(int i = 0; i < m; i++) {
					if(IsEdgeBlocked(i)) continue;

					std::pair<int, int> p = G.GetEdge(i);
					int u = p.first;
					int v = p.second;

					degree[u]++;
					degree[v]++;
				}

				for(int i = 0; i < n; i++)
					B.Insert(degree[i], i);

				while(B.Size() > 0) {
					int u = B.DeleteMin();
					if(mate[outer[u]] == -1) {
						int min = -1;
						for(std::list<int>::const_iterator it = G.AdjList(u).begin(); it != G.AdjList(u).end(); it++) {
							int v = *it;

							if(IsEdgeBlocked(u, v) or
									(outer[u] == outer[v]) or
									(mate[outer[v]] != -1) )
								continue;

							if(min == -1 or degree[v] < degree[min])
								min = v;
						}
						if(min != -1) {
							mate[outer[u]] = min;
							mate[outer[min]] = u;
						}
					}
				}
			}

			//Modifies the costs of the graph so the all edges have positive costs
			void PositiveCosts() {
				double minEdge = 0;
				for(int i = 0; i < m ;i++)
					if(GREATER(minEdge - slack[i], 0))
						minEdge = slack[i];

				for(int i = 0; i < m; i++)
					slack[i] -= minEdge;
			}

			std::list<int> RetrieveMatching() {
				std::list<int> matching;

				for(int i = 0; i < 2*n; i++)
					if(active[i] and mate[i] != -1 and outer[i] == i)
						Expand(i, true);

				for(int i = 0; i < m; i++) {
					int u = G.GetEdge(i).first;
					int v = G.GetEdge(i).second;

					if(mate[u] == v)
						matching.push_back(i);
				}
				return matching;
			}

			int GetFreeBlossomIndex() {
				int i = free.back();
				free.pop_back();
				return i;
			}

			void AddFreeBlossomIndex(int i) {
				free.push_back(i);
			}

			void ClearBlossomIndices() {
				free.clear();
				for(int i = n; i < 2*n; i++)
					AddFreeBlossomIndex(i);
			}

			//An edge might be blocked due to the dual costs
			inline bool IsEdgeBlocked(int u, int v) {
				return GREATER(slack[ G.GetEdgeIndex(u, v) ], 0);
			}

			inline bool IsEdgeBlocked(int e) {
				return GREATER(slack[e], 0);
			}

			//Returns true if u and v are adjacent in G and not blocked
			inline bool IsAdjacent(int u, int v) {
				return (G.AdjMat()[u][v] and not IsEdgeBlocked(u, v));
			}

			const Graph & G;

			std::list<int> free;//List of free blossom indices

			std::vector<int> outer;//outer[v] gives the index of the outermost blossom that contains v, outer[v] = v if v is not contained in any blossom
			std::vector< std::list<int> > deep;//deep[v] is a list of all the original vertices contained inside v, deep[v] = v if v is an original vertex
			std::vector< std::list<int> > shallow;//shallow[v] is a list of the vertices immediately contained inside v, shallow[v] is empty is the default
			std::vector<int> tip;//tip[v] is the tip of blossom v
			std::vector<bool> active;//true if a blossom is being used

			std::vector<int> type;//Even, odd, neither (2, 1, 0)
			std::vector<int> forest;//forest[v] gives the father of v in the alternating forest
			std::vector<int> root;//root[v] gives the root of v in the alternating forest

			std::vector<bool> blocked;//A blossom can be blocked due to dual costs, this means that it behaves as if it were an original vertex and cannot be expanded
			std::vector<double> dual;//dual multipliers associated to the blossoms, if dual[v] > 0, the blossom is blocked and full
			std::vector<double> slack;//slack associated to each edge, if slack[e] > 0, the edge cannot be used
			std::vector<int> mate;//mate[v] gives the mate of v

			int m, n;

			bool perfect;

			std::list<int> forestList;
			std::vector<int> visited;
	};

} // namespace mcpm
