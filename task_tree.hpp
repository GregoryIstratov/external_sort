#pragma once

#include <memory>
#include <list>

#include "chunk.hpp"
#include "util.hpp"
#include "task.hpp"

template<typename T>
struct task_tree_node
{
        std::unique_ptr<chunk_merge_task<T>> task;

        std::list<std::shared_ptr<task_tree_node>> childs;
        std::shared_ptr<task_tree_node> parent;
};

template<typename T>
class task_tree
{
public:
        void build(std::list<chunk_id> l0_ids, size_t base)
        {
                base_ = base;

                chunk_id::id_t id_idx   = 0;
                chunk_id::lvl_t lvl_idx = 1;

                std::list<std::shared_ptr<task_tree_node<T>>> nodes;

                while(!l0_ids.empty())
                {
                        size_t q_size = l0_ids.size();

                        size_t n = std::min(base_, q_size);

                        size_t rem = q_size - n;
                        if(0 < rem && rem < base_)
                                n += rem;

                        std::list<chunk_id> ids;

                        auto end = l0_ids.begin();
                        std::advance(end, n);

                        ids.splice(ids.begin(), l0_ids, l0_ids.begin(), end);

                        if(ids.empty())
                                break;


                        chunk_id output_id(lvl_idx, id_idx++);
                        auto task = make_task(std::move(ids), output_id);

                        auto node = std::make_shared<task_tree_node<T>>();
                        node->task = std::move(task);

                        nodes.push_back(std::move(node));
                }

                root_ = build(std::move(nodes), ++lvl_idx);
        }

        std::list<std::unique_ptr<chunk_merge_task<T>>> make_queue()
        {
                std::list<std::shared_ptr<task_tree_node<T>>> q;
                std::list<std::unique_ptr<chunk_merge_task<T>>> q2;

                q.push_back(root_);

                while(!q.empty())
                {
                        auto node = q.front();

                        q.pop_front();

                        q2.push_front(std::move(node->task));

                        for(auto& c : node->childs)
                        {
                                q.push_back(c);
                        }
                }

                return std::move(q2);
        }


private:

        std::shared_ptr<task_tree_node<T>>
        build(std::list<std::shared_ptr<task_tree_node<T>>>&& nodes,
              chunk_id::lvl_t lvl)
        {
                std::list<std::shared_ptr<task_tree_node<T>>> new_nodes;
                chunk_id::id_t id = 0;
                while(!nodes.empty())
                {
                        size_t q_size = nodes.size();

                        size_t n = std::min(base_, q_size);

                        size_t rem = q_size - n;
                        if(0 < rem && rem < base_)
                                n += rem;

                        std::list<std::shared_ptr<task_tree_node<T>>> childs;

                        auto end = nodes.begin();
                        std::advance(end, n);

                        childs.splice(childs.begin(), nodes, nodes.begin(), end);

                        if(childs.empty())
                                break;

                        chunk_id output_id(lvl, id++);

                        std::vector<chunk_istream<T>> chunks;
                        auto new_node = std::make_shared<task_tree_node<T>>();

                        for(auto& node : childs)
                        {
                                chunks.emplace_back(node->task->id());

                                node->parent = new_node;
                        }

                        std::string name = make_filename(output_id);
                        chunk_ostream<T> os(std::move(name));


                        new_node->task = std::make_unique<chunk_merge_task<T>>(std::move(chunks),
                                std::move(os),
                                output_id);

                        new_node->childs = std::move(childs);

                        new_nodes.push_back(std::move(new_node));
                }

                if(new_nodes.size() > 1)
                        return build(std::move(new_nodes), ++lvl);
                else
                        return new_nodes.back();
        }


        std::unique_ptr<chunk_merge_task<T>> make_task(std::list<chunk_id>&& ids,
                                                       chunk_id output_id)
        {
                std::vector<chunk_istream<T>> chunks;

                for(const chunk_id& id : ids)
                {
                        chunks.emplace_back(id);
                }

                std::string name = make_filename(output_id);
                chunk_ostream<T> os(std::move(name));

                return std::make_unique<chunk_merge_task<T>>(std::move(chunks),
                        std::move(os),
                        output_id);
        }

private:
        size_t base_;
        std::shared_ptr<task_tree_node<T>> root_;
};
