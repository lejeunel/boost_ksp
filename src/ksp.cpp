/*
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Laurent Lejeune (laurent.lejeune@artorg.unibe.ch).
 *
 */
#include "ksp.h"

Ksp::Ksp() { new_graph(); }

void Ksp::config(vertex_id_type source_vertex_id, vertex_id_type sink_vertex_id,
                 int a_l_max, std::string source_vertex_name,
                 std::string sink_vertex_name, std::string loglevel,
                 bool a_min_cost, bool a_return_edges) {
  /* Define:
     - source_vertex_id: id of source vertex
     - sink_vertex_id: id of source vertex
     - l_max: Max number of paths to compute. -1 for no limit
     - source_vertex_name: Name of source vertex (optional)
     - sink_vertex_name: Name of sink vertex (optional)
     - loglevel: What to print? (see globals.h)
     - min_cost: False for saturation of graph. True to stop at minimum cost
   */

  set_source(source_vertex_id, source_vertex_name);
  set_sink(sink_vertex_id, sink_vertex_name);

  if (a_l_max == -1)
    l_max = std::numeric_limits<int>::max();
  else
    l_max = a_l_max;

  set_loglevel(loglevel);
  min_cost = a_min_cost;

  return_edges = a_return_edges;
}

void Ksp::print_all() { utils::print_all(*G); }

VertexDesc Ksp::add_vertex(vertex_id_type id, std::string str) {
  return utils::add_vertex(*G, id, str);
}

void Ksp::remove_vertex(vertex_id_type u_id) {
  VertexDesc u;
  VertexIter wi, wi_end;
  for (boost::tie(wi, wi_end) = vertices(*G); wi != wi_end; ++wi) {
    if ((*G)[*wi].id == u_id) {
      u = *wi;
      boost::remove_vertex(u, *G);
      return;
    }
  }
}

void Ksp::clear_vertex(vertex_id_type u_id) {
  VertexDesc u;
  VertexIter wi, wi_end;
  for (boost::tie(wi, wi_end) = vertices(*G); wi != wi_end; ++wi) {
    if ((*G)[*wi].id == u_id) {
      u = *wi;
      boost::clear_vertex(u, *G);
      return;
    }
  }
}

bp::list Ksp::out_edges(vertex_id_type u_id) {

  bp::list out;

  VertexDesc u;
  std::pair<VertexIter, VertexIter> vp;
  bool found_vertex = false;
  for (vp = vertices(*G); vp.first != vp.second; ++vp.first)
    if ((*G)[*(vp.first)].id == u_id) {
      u = *(vp.first);
      found_vertex = true;
    }

  if (!found_vertex)
    return out;

  OutEdgeIter ei, ei_end;
  VertexDesc source;
  VertexDesc target;
  for (boost::tie(ei, ei_end) = boost::out_edges(u, *G); ei != ei_end; ++ei) {
    source = boost::source(*ei, *G);
    target = boost::target(*ei, *G);

    out.append(bp::make_tuple<vertex_id_type, vertex_id_type>((*G)[source].id,
                                                              (*G)[target].id));
  }

  return out;
}

void Ksp::remove_edge(vertex_id_type u_id, vertex_id_type v_id) {

  VertexDesc u = 0;
  VertexIter wi, wi_end;
  for (boost::tie(wi, wi_end) = vertices(*G); wi != wi_end; ++wi) {
    if ((*G)[*wi].id == u_id)
      u = *wi;
  }

  VertexDesc v = 0;
  for (boost::tie(wi, wi_end) = vertices(*G); wi != wi_end; ++wi) {
    if ((*G)[*wi].id == v_id)
      v = *wi;
  }

  std::pair<EdgeDesc, bool> e = boost::edge(u, v, *G);
  if (e.second)
    boost::remove_edge(u, v, *G);
}

bool Ksp::add_edge(vertex_id_type n0, vertex_id_type n1, double w,
                   edge_id_type id, std::string str_0, std::string str_1,
                   int label) {

  return utils::add_edge(*G, n0, n1, w, id, str_0, str_1, label);
}

void Ksp::set_source(vertex_id_type id, std::string name) {
  source_vertex = utils::add_vertex(*G, id, name);
}

void Ksp::set_sink(vertex_id_type id, std::string name) {
  sink_vertex = utils::add_vertex(*G, id, name);
}

vertex_id_type Ksp::num_vertices() { return boost::num_vertices(*G); }

edge_id_type Ksp::num_edges() { return boost::num_edges(*G); }

void Ksp::new_graph() {

  n_vertices = 0;
  G = new MyGraph(n_vertices);
  (*G)[graph_bundle].name = "defaultName";
}

void Ksp::set_loglevel(std::string a_log_level) {

  using namespace boost::log::trivial;
  logSeverityLevel log_level;

  if (a_log_level == "trace")
    log_level = logSeverityLevel::trace;
  else if (a_log_level == "debug")
    log_level = logSeverityLevel::debug;
  else if (a_log_level == "info")
    log_level = logSeverityLevel::info;
  else if (a_log_level == "warning")
    log_level = logSeverityLevel::warning;
  else if (a_log_level == "error")
    log_level = logSeverityLevel::error;
  else
    log_level = logSeverityLevel::info;

  boost::log::core::get()->set_filter(boost::log::trivial::severity >=
                                      log_level);
}

bp::list Ksp::run() {
  // Runs the edge-disjoint K-shortest path

  // Make copy of graph for cost transformation
  G_c = new MyGraph(0);

  copy_graph(*G, *G_c);
  (*G_c)[graph_bundle].name = (*G)[graph_bundle].name + "_cost_transform";

  // Store output of shortest-paths
  bool res_ok;

  std::vector<double> res_distance(num_vertices(),
                                (std::numeric_limits<double>::max)());

  auto v_index = get(boost::vertex_index, *G);
  res_distance[v_index[source_vertex]] = 0;

  EdgeSets P(*G);
  EdgeSets P_prev(*G);
  EdgeSet res_path(*G);
  EdgeSet p_inter(*G_c);

  BOOST_LOG_TRIVIAL(debug) << "graph: ";
  utils::print_all(*G);

  BOOST_LOG_TRIVIAL(debug) << "Bellman-Ford...";
  bellman_ford_shortest_paths(*G, res_path, res_ok, res_distance);

  if (!res_ok) {
    BOOST_LOG_TRIVIAL(error) << "Couldn't compute a single path!";
  } else {
    BOOST_LOG_TRIVIAL(debug) << "...ok";
    P += res_path;
    cost = utils::calc_cost(P, *G);
    BOOST_LOG_TRIVIAL(info) << "l: " << 0 << ", cost: " << cost;

    utils::print_paths(P, *G);
  }

  // invert edges of Bellman-Ford solution
  utils::invert_edges(EdgeSets(P).convert_to_graph(*G_c), true, true, true, *G_c);

  // Store solution
  P_prev = P;

  for (int l = 1; l < l_max; ++l) {

    EdgeSet p_cut(*G);

    // Transform costs. Will make all weights non-negative
    cost_transform(res_distance, *G_c);
    utils::print_all(*G_c);
    BOOST_LOG_TRIVIAL(debug) << "Dijkstra...";
    std::tie(p_inter, res_ok, res_distance) =
        dijkstra_shortest_paths(*G_c, source_vertex, sink_vertex);

    // p_cut are invalid edges, i.e. will be excluded from augmentation set
    p_cut = p_inter.get_invalid_edges(*G);
    utils::print_path(p_inter, *G_c);
    p_inter.convert_to_graph(*G);

    // convert p_cut to valid edges on *G
    EdgeSet p_cut_g = EdgeSet(p_cut).convert_to_graph(*G);

    if (res_ok) {

      // If interlacing path didn't cut edges of previous solution,
      // add path as is, else augment
      if (p_cut.size() > 0) {
        utils::set_label_to_invalid_edges(p_cut, *G, 0, true);
        BOOST_LOG_TRIVIAL(debug) << "num invalid edges (cut) : "
                                 << utils::num_edges_with_label(*G, 0);
        BOOST_LOG_TRIVIAL(debug) << "p_cut: ";
        utils::print_path(p_cut, *G_c);
        utils::print_all(*G);

        BOOST_LOG_TRIVIAL(debug) << "Augmenting";
        p_inter.convert_to_graph(*G);
        P = utils::augment(P, p_cut_g, p_inter, sink_vertex, *G);
      } else {
        P += p_inter;
        utils::set_label(p_inter, *G, -P.size());
      }
      new_cost = utils::calc_cost(P, *G);
      BOOST_LOG_TRIVIAL(info) << "l: " << l << ", cost: " << new_cost;

      utils::print_paths(P, *G);
      EdgeSets tmp = EdgeSets(P);
      tmp.convert_to_graph(*G_c);
      utils::invert_edges(tmp, true, true, true, *G_c);

      // set label=1 and invert back edges of p_cut
      utils::set_label(p_cut, *G_c, 1);
      utils::invert_edges(p_cut, false, false, false, *G_c);

      // reset label on invalid edges
      utils::set_label_to_invalid_edges(p_cut, *G, 1, true);

      if (min_cost && (new_cost > cost)) {
        BOOST_LOG_TRIVIAL(info) << "Reached minimum at l= " << l - 1;
        return utils::edgeSets_to_edges_list(P_prev, *G);

      } else {
        P_prev = P;
        cost = new_cost;
      }
    } else {
      BOOST_LOG_TRIVIAL(info) << "Stopped at l= " << l - 1;

      break;
    }
  }

  // Re-initialize edge labels on original graph
  utils::set_label_to_all(*G, 1);

  // Free memory of G_c
  delete G_c;

  utils::print_all(*G);
  if (return_edges) {
    bp::list out_list = utils::edgeSets_to_edges_list(P, *G);
    return out_list;
  } else {
    bp::list out_list = utils::edgeSets_to_vertices_list(P, *G);
    return out_list;
  }
}

std::tuple<EdgeSet, bool, std::vector<double>>
Ksp::dijkstra_shortest_paths(const MyGraph &g, VertexDesc source_vertex,
                             VertexDesc sink_vertex) {

  EdgeSet out_path(g);

  // init the distance
  std::vector<double> distances(num_vertices());

  std::vector<std::size_t> predecessors(num_vertices());

  // my_visitor vis(sink_vertex);

  VertexPath shortest;
  bool r;

  // check if there are out-edge from source
  int n_out_edges = boost::out_degree(source_vertex, g);
  if (n_out_edges == 0) {
    BOOST_LOG_TRIVIAL(debug) << "found no out edges from source...";
    r = false;
  } else {
    BOOST_LOG_TRIVIAL(debug)
        << "found " << n_out_edges << " out edges from source...";
    try {
      boost::dijkstra_shortest_paths(
          g, source_vertex,
          weight_map(get(&EdgeProperty::weight, g))
              .predecessor_map(boost::make_iterator_property_map(
                  predecessors.begin(), get(vertex_index, g)))
              .distance_map(make_iterator_property_map(distances.begin(),
                                                       get(vertex_index, g)))
          //.visitor(vis)
      );
      std::tie(shortest, r) =
          utils::pred_to_path(predecessors, g, source_vertex, sink_vertex);
    } catch (int exception) {
      BOOST_LOG_TRIVIAL(debug) << "dijktra failed to visit all nodes";
      r = false;
    }
  }

  if (!r) {
    BOOST_LOG_TRIVIAL(debug) << "Couldn't reach sink node";
    out_path = EdgeSet(g);
    r = false;
  } else {
    out_path = utils::vertpath_to_edgepath(shortest, g);
    // utils::print_path(out_path, g);
    r = true;
  }

  return std::make_tuple(out_path, r, distances);
}

void
Ksp::bellman_ford_shortest_paths(const MyGraph &g, EdgeSet & res_path,
                                 bool & res_ok,
                                 std::vector<double> & res_distance ) {

  std::pair<VertexIter, VertexIter> vp;
  // auto v_index = get(boost::vertex_index, g);
  auto weight = boost::make_transform_value_property_map(
      std::mem_fn(&EdgeProperty::weight), get(boost::edge_bundle, g));

  // init
  std::vector<VertexDesc> predecessors(num_vertices());

  auto v_index = get(boost::vertex_index, *G);
  res_ok = boost::bellman_ford_shortest_paths(
      *G, num_vertices(),
      weight_map(weight)
          .distance_map(make_iterator_property_map(res_distance.begin(),
                                                   v_index))
          .predecessor_map(
              make_iterator_property_map(predecessors.begin(),
                                         v_index)));

  VertexPath shortest;

  if (res_ok) {

    std::tie(shortest, std::ignore) =
        utils::pred_to_path(predecessors, g, source_vertex, sink_vertex);
    res_path = utils::vertpath_to_edgepath(shortest, g);
  }
  else {
    BOOST_LOG_TRIVIAL(info) << "negative cycle";
  }
}

void Ksp::cost_transform(const std::vector<double> &distance, MyGraph &g_out) {

  EdgeIter ei, ei_end;
  double s_i;
  double s_j;
  double w_ij;
  for (boost::tie(ei, ei_end) = edges(g_out); ei != ei_end; ++ei) {
    s_i = distance[source(*ei, g_out)];
    s_j = distance[target(*ei, g_out)];
    w_ij = g_out[*ei].weight;
    BOOST_LOG_TRIVIAL(trace) << "e: " << g_out[source(*ei, g_out)].id << ", "
                             << g_out[target(*ei, g_out)].id;
    BOOST_LOG_TRIVIAL(trace) << "s_i: " << s_i;
    BOOST_LOG_TRIVIAL(trace) << "s_j: " << s_j;
    BOOST_LOG_TRIVIAL(trace) << "w_ij: " << w_ij;
    g_out[*ei].weight = w_ij + s_i - s_j;
  }
}

void Ksp::set_label_all_edges(int label) { utils::set_label_to_all(*G, label); }
