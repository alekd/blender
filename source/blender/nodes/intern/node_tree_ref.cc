/*
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
 */

#include "NOD_node_tree_ref.hh"

#include "BLI_dot_export.hh"

namespace blender::nodes {

NodeTreeRef::NodeTreeRef(bNodeTree *btree) : btree_(btree)
{
  Map<bNode *, NodeRef *> node_mapping;

  LISTBASE_FOREACH (bNode *, bnode, &btree->nodes) {
    NodeRef &node = *allocator_.construct<NodeRef>();

    node.tree_ = this;
    node.bnode_ = bnode;
    node.id_ = nodes_by_id_.append_and_get_index(&node);
    RNA_pointer_create(&btree->id, &RNA_Node, bnode, &node.rna_);

    LISTBASE_FOREACH (bNodeSocket *, bsocket, &bnode->inputs) {
      InputSocketRef &socket = *allocator_.construct<InputSocketRef>();
      socket.node_ = &node;
      socket.index_ = node.inputs_.append_and_get_index(&socket);
      socket.is_input_ = true;
      socket.bsocket_ = bsocket;
      socket.id_ = sockets_by_id_.append_and_get_index(&socket);
      RNA_pointer_create(&btree->id, &RNA_NodeSocket, bsocket, &socket.rna_);
    }

    LISTBASE_FOREACH (bNodeSocket *, bsocket, &bnode->outputs) {
      OutputSocketRef &socket = *allocator_.construct<OutputSocketRef>();
      socket.node_ = &node;
      socket.index_ = node.outputs_.append_and_get_index(&socket);
      socket.is_input_ = false;
      socket.bsocket_ = bsocket;
      socket.id_ = sockets_by_id_.append_and_get_index(&socket);
      RNA_pointer_create(&btree->id, &RNA_NodeSocket, bsocket, &socket.rna_);
    }

    input_sockets_.extend(node.inputs_.as_span());
    output_sockets_.extend(node.outputs_.as_span());

    node_mapping.add_new(bnode, &node);
  }

  LISTBASE_FOREACH (bNodeLink *, blink, &btree->links) {
    OutputSocketRef &from_socket = this->find_output_socket(
        node_mapping, blink->fromnode, blink->fromsock);
    InputSocketRef &to_socket = this->find_input_socket(
        node_mapping, blink->tonode, blink->tosock);

    LinkRef &link = *allocator_.construct<LinkRef>();
    link.from_ = &from_socket;
    link.to_ = &to_socket;
    link.blink_ = blink;

    links_.append(&link);
    from_socket.directly_linked_sockets_.append(&to_socket);
    to_socket.directly_linked_sockets_.append(&from_socket);
    from_socket.directly_linked_links_.append(&link);
    to_socket.directly_linked_links_.append(&link);
  }

  for (OutputSocketRef *socket : output_sockets_) {
    if (!socket->node_->is_reroute_node()) {
      this->find_targets_skipping_reroutes(*socket, socket->linked_sockets_);
      for (SocketRef *target : socket->linked_sockets_) {
        target->linked_sockets_.append(socket);
      }
    }
  }

  for (NodeRef *node : nodes_by_id_) {
    const bNodeType *nodetype = node->bnode_->typeinfo;
    nodes_by_type_.add(nodetype, node);
  }
}

NodeTreeRef::~NodeTreeRef()
{
  /* The destructor has to be called manually, because these types are allocated in a linear
   * allocator. */
  for (NodeRef *node : nodes_by_id_) {
    node->~NodeRef();
  }
  for (InputSocketRef *socket : input_sockets_) {
    socket->~InputSocketRef();
  }
  for (OutputSocketRef *socket : output_sockets_) {
    socket->~OutputSocketRef();
  }
  for (LinkRef *link : links_) {
    link->~LinkRef();
  }
}

InputSocketRef &NodeTreeRef::find_input_socket(Map<bNode *, NodeRef *> &node_mapping,
                                               bNode *bnode,
                                               bNodeSocket *bsocket)
{
  NodeRef *node = node_mapping.lookup(bnode);
  for (InputSocketRef *socket : node->inputs_) {
    if (socket->bsocket_ == bsocket) {
      return *socket;
    }
  }
  BLI_assert(false);
  return *node->inputs_[0];
}

OutputSocketRef &NodeTreeRef::find_output_socket(Map<bNode *, NodeRef *> &node_mapping,
                                                 bNode *bnode,
                                                 bNodeSocket *bsocket)
{
  NodeRef *node = node_mapping.lookup(bnode);
  for (OutputSocketRef *socket : node->outputs_) {
    if (socket->bsocket_ == bsocket) {
      return *socket;
    }
  }
  BLI_assert(false);
  return *node->outputs_[0];
}

void NodeTreeRef::find_targets_skipping_reroutes(OutputSocketRef &socket,
                                                 Vector<SocketRef *> &r_targets)
{
  for (SocketRef *direct_target : socket.directly_linked_sockets_) {
    if (direct_target->node_->is_reroute_node()) {
      this->find_targets_skipping_reroutes(*direct_target->node_->outputs_[0], r_targets);
    }
    else {
      r_targets.append_non_duplicates(direct_target);
    }
  }
}

static bool has_link_cycles_recursive(const NodeRef &node,
                                      MutableSpan<bool> visited,
                                      MutableSpan<bool> is_in_stack)
{
  const int node_id = node.id();
  if (is_in_stack[node_id]) {
    return true;
  }
  if (visited[node_id]) {
    return false;
  }

  visited[node_id] = true;
  is_in_stack[node_id] = true;

  for (const OutputSocketRef *from_socket : node.outputs()) {
    for (const InputSocketRef *to_socket : from_socket->directly_linked_sockets()) {
      const NodeRef &to_node = to_socket->node();
      if (has_link_cycles_recursive(to_node, visited, is_in_stack)) {
        return true;
      }
    }
  }

  is_in_stack[node_id] = false;
  return false;
}

bool NodeTreeRef::has_link_cycles() const
{
  const int node_amount = nodes_by_id_.size();
  Array<bool> visited(node_amount, false);
  Array<bool> is_in_stack(node_amount, false);

  for (const NodeRef *node : nodes_by_id_) {
    if (has_link_cycles_recursive(*node, visited, is_in_stack)) {
      return true;
    }
  }
  return false;
}

std::string NodeTreeRef::to_dot() const
{
  dot::DirectedGraph digraph;
  digraph.set_rankdir(dot::Attr_rankdir::LeftToRight);

  Map<const NodeRef *, dot::NodeWithSocketsRef> dot_nodes;

  for (const NodeRef *node : nodes_by_id_) {
    dot::Node &dot_node = digraph.new_node("");
    dot_node.set_background_color("white");

    Vector<std::string> input_names;
    Vector<std::string> output_names;
    for (const InputSocketRef *socket : node->inputs()) {
      input_names.append(socket->name());
    }
    for (const OutputSocketRef *socket : node->outputs()) {
      output_names.append(socket->name());
    }

    dot_nodes.add_new(node,
                      dot::NodeWithSocketsRef(dot_node, node->name(), input_names, output_names));
  }

  for (const OutputSocketRef *from_socket : output_sockets_) {
    for (const InputSocketRef *to_socket : from_socket->directly_linked_sockets()) {
      dot::NodeWithSocketsRef &from_dot_node = dot_nodes.lookup(&from_socket->node());
      dot::NodeWithSocketsRef &to_dot_node = dot_nodes.lookup(&to_socket->node());

      digraph.new_edge(from_dot_node.output(from_socket->index()),
                       to_dot_node.input(to_socket->index()));
    }
  }

  return digraph.to_dot_string();
}

const NodeTreeRef &get_tree_ref_from_map(NodeTreeRefMap &node_tree_refs, bNodeTree &btree)
{
  return *node_tree_refs.lookup_or_add_cb(&btree,
                                          [&]() { return std::make_unique<NodeTreeRef>(&btree); });
}

}  // namespace blender::nodes
