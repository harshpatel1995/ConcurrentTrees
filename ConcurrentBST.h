
#include <algorithm>
#include <iostream>
#include <thread>
#include <mutex>

#include "HolderMutex.h"

template<typename T>
class ConcurrentAVLTree
{
    template<typename G>
    struct ConcurrentNode
    {
        const G data; // immutable
        bool valid = true;
        ConcurrentNode<G> *left = NULL;
        ConcurrentNode<G> *right = NULL;
        ConcurrentNode<G> *parent = NULL;
        ConcurrentNode<G> *pred = NULL;
        ConcurrentNode<G> *succ = NULL;

        int left_tree_height = 0;
        int right_tree_height = 0;

        HolderMutex tree_lock;
        HolderMutex succ_lock;

        ConcurrentNode<G>(const G data, ConcurrentNode<G> *pred, ConcurrentNode<G> *succ, ConcurrentNode<G> *parent) :
            data(data),
            parent(parent),
            pred(pred),
            succ(succ)
        {
        }
    };

public:
    ConcurrentAVLTree()
    {
        auto parent = new ConcurrentNode<T>(-100000, NULL, NULL, NULL);
        _root = new ConcurrentNode<T>(100000, parent, parent, parent); // todo: change magic num

        parent->right = _root;
        parent->succ = _root;
    }

    ~ConcurrentAVLTree()
    {
        deleteTree(_root);
    }

    void print() const
    {
        printRecursive(_root);
    }

    bool contains(T data) const
    {
        auto node = search(data);
        while (node->data > data) node = node->pred;
        while (node->data < data) node = node->succ;
        return (node->data == data) && node->valid;
    }

    bool insert(T data)
    {
        while (true)
        {
            auto node = search(data);
            int res = data - node->data;
            auto pred = (res > 0) ? node : node->pred;

            try
            {
                pred->succ_lock.lock();

                if (pred->valid)
                {
                    auto pred_value = pred->data;
                    int pred_res = (pred == node ? res : data - pred_value);

                    if (pred_res > 0)
                    {
                        auto succ = pred->succ;
                        auto succ_value = succ->data;
                        int res2 = (succ == node ? res : data - succ_value);
                        if (res2 <= 0)
                        {
                            if (res2 == 0)
                            {
                                pred->succ_lock.unlock();
                                return false;
                            }

                            auto parent = chooseParent(pred, succ, node);
                            auto newNode = new ConcurrentNode<T>(data, pred, succ, parent);

                            succ->pred = newNode;
                            pred->succ = newNode;
                            pred->succ_lock.unlock();
                            insertToTree(parent, newNode, parent == pred);
                            return true;
                        }
                    }
                }

                pred->succ_lock.unlock();
            }
            catch (const std::exception &e)
            {
                std::cout << "Error insert: " << e.what() << std::endl;
                pred->succ_lock.unlock();
                return false;
            }
        }

        // should never reach
        return true;
    }


    bool remove(T data)
    {
        while (true)
        {
            auto node = search(data);
            int res = data - node->data;
            auto pred = (res > 0) ? node : node->pred;

            try
            {
                pred->succ_lock.lock();

                if (pred->valid)
                {
                    auto pred_value = pred->data;
                    int pred_res = (pred == node) ? res : data - pred_value;
                    if (pred_res > 0)
                    {
                        auto succ = pred->succ;
                        auto succ_value = succ->data;
                        int res2 = (succ == node) ? res : data - succ_value;

                        if (res2 <= 0)
                        {
                            if (res2 != 0)
                            {
                                pred->succ_lock.unlock();
                                return false;
                            }

                            succ->succ_lock.lock();
                            auto successor = acquireTreeLocks(succ);
                            auto succParent = lockParent(succ);

                            succ->valid = false;

                            auto succ_succ = succ->succ;
                            succ_succ->pred = pred;
                            pred->succ = succ_succ;
                            succ->succ_lock.unlock();
                            pred->succ_lock.unlock();

                            removeFromTree(succ, successor, succParent);
                            return true;
                        }
                    }
                }
                pred->succ_lock.unlock();
            }

            catch (const std::exception& e)
            {
                std::cout << "Error remove: " << e.what() << std::endl;
                pred->succ_lock.unlock();
            }
        }

        return true;
    }

private:
    ConcurrentNode<T> *_root;

    ConcurrentNode<T>* search(T data) const
    {
        ConcurrentNode<T> *node = _root;
        ConcurrentNode<T> *child;

        while (true)
        {
            auto curr_data = node->data;
            if (curr_data == data) break;

            child = (curr_data < data) ? node->right : node->left;

            if (child == NULL) break;

            node = child;
        }

        return node;
    }

    ConcurrentNode<T>* chooseParent(ConcurrentNode<T> *pred, ConcurrentNode<T> *succ, ConcurrentNode<T> *node)
    {
        auto candidate = (node == pred || node == succ) ? node : pred;

        while (true)
        {
            candidate->tree_lock.lock();

            if (candidate == pred)
            {
                if (!candidate->right) return candidate;
                candidate->tree_lock.unlock();
                candidate = succ;
            }
            else
            {
                if (!candidate->left) return candidate;
                candidate->tree_lock.unlock();
                candidate = pred;
            }

            std::this_thread::yield();
        }

        return NULL;
    }

    void insertToTree(ConcurrentNode<T>* parent, ConcurrentNode<T>* new_node, bool is_right)
    {
        if (is_right)
        {
            parent->right = new_node;
            parent->right_tree_height = 1;
        }
        else
        {
            parent->left = new_node;
            parent->left_tree_height = 1;
        }
        if (parent != _root)
        {
            auto grand_parent = lockParent(parent);
            rebalance(grand_parent, parent, grand_parent->left == parent);
        }
        else parent->tree_lock.unlock();
    }

    ConcurrentNode<T>* acquireTreeLocks(ConcurrentNode<T>* node)
    {
        while (true)
        {
            node->tree_lock.lock();
            auto right = node->right;
            auto left = node->left;

            if (!right || !left)
            {
                if (right && !right->tree_lock.try_lock())
                {
                    node->tree_lock.unlock();
                    std::this_thread::yield();
                    continue;
                }
                if (left && !left->tree_lock.try_lock())
                {
                    node->tree_lock.unlock();
                    std::this_thread::yield();
                    continue;
                }
                return NULL;
            }

            auto succ = node->succ;
            auto parent = succ->parent;
            if (parent != node)
            {
                if (!parent->tree_lock.try_lock())
                {
                    node->tree_lock.unlock();
                    std::this_thread::yield();
                    continue;
                }
                else if (parent != succ->parent || !parent->valid)
                {
                    parent->tree_lock.unlock();
                    node->tree_lock.unlock();
                    std::this_thread::yield();
                    continue;
                }
            }

            if (!succ->tree_lock.try_lock())
            {
                node->tree_lock.unlock();
                if (parent != node)
                {
                    parent->tree_lock.unlock();
                }
                std::this_thread::yield();
                continue;
            }

            auto succ_right_child = succ->right;

            if (succ_right_child && !succ_right_child->tree_lock.try_lock())
            {
                node->tree_lock.unlock();
                succ->tree_lock.unlock();
                if (parent != node)
                {
                    parent->tree_lock.unlock();
                }
                std::this_thread::yield();
                continue;
            }

            return succ;
        }
    }

    void removeFromTree(ConcurrentNode<T>* node, ConcurrentNode<T>* succ, ConcurrentNode<T>* parent)
    {
        if (!succ)
        {
            auto right = node->right;
            auto child = (right == NULL) ? node->left : right;

            bool left = updateChild(parent, node, child);
            node->tree_lock.unlock();
            rebalance(parent, child, left);
            return;
        }

        auto old_parent = succ->parent;
        auto old_right = succ->right;
        updateChild(old_parent, succ, old_right);

        succ->left_tree_height = node->left_tree_height;
        succ->right_tree_height = node->right_tree_height;
        auto left = node->left;
        auto right = node->right;
        succ->parent = parent;
        succ->left = node->left;
        succ->right = node->right;
        left->parent = succ;

        if (node->right) right->parent = succ;

        if (parent->left == node) parent->left = succ;
        else parent->right = succ;

        bool is_left = (old_parent != node);
        bool violated = abs(getBalanceFactor(succ)) >= 2;

        if (!is_left) old_parent = succ;
        else succ->tree_lock.unlock();

        node->tree_lock.unlock();
        parent->tree_lock.unlock();

        rebalance(old_parent, old_right, is_left);

        if (violated)
        {
            succ->tree_lock.lock();
            int bf = getBalanceFactor(succ);
            if (succ->valid && abs(bf) >= 2) rebalance(succ, NULL, bf >= 2 ? false : true);
            else succ->tree_lock.unlock();
        }
    }

    bool updateChild(ConcurrentNode<T>* parent, ConcurrentNode<T>* old_child, ConcurrentNode<T>* new_child)
    {
        if (new_child) new_child->parent = parent;
        bool left = parent->left == old_child;
        if (left) parent->left = new_child;
        else parent->right = new_child;
        return left;
    }

    ConcurrentNode<T>* lockParent(ConcurrentNode<T>* node)
    {
        auto parent = node->parent;

        try
        {
            parent->tree_lock.lock();

            while (node->parent != parent || !parent->valid)
            {
                parent->tree_lock.unlock();
                parent = node->parent;

                while (!parent->valid)
                {
                    std::this_thread::yield();
                    parent = node->parent;
                }

                parent->tree_lock.lock();
            }

            return parent;
        }

        catch (const std::exception& e)
        {
            parent->tree_lock.unlock();
            std::cout << "Error: lockParent: " << e.what() << std::endl;
            return NULL;
        }
    }

    void unlockRebalance(ConcurrentNode<T>* node, ConcurrentNode<T>* child, ConcurrentNode<T>* parent)
    {
        if (child && child->tree_lock.owns_lock()) child->tree_lock.unlock();
        if (node && node->tree_lock.owns_lock()) node->tree_lock.unlock();
        if (parent && parent->tree_lock.owns_lock()) parent->tree_lock.unlock();
    }

    int getBalanceFactor(ConcurrentNode<T>* node)
    {
        return node->left_tree_height - node->right_tree_height;
    }

    bool updateHeight(ConcurrentNode<T> *child, ConcurrentNode<T> *node, bool is_left)
    {
        int new_height = (child == NULL) ? 0 : std::max(child->left_tree_height, child->right_tree_height) + 1;
        int old_height = is_left ? node->left_tree_height : node->right_tree_height;

        if (new_height == old_height) return false;

        if (is_left)
            node->left_tree_height = new_height;
        else
            node->right_tree_height = new_height;

        return true;
    }

    ConcurrentNode<T>* restart(ConcurrentNode<T>* node, ConcurrentNode<T>* parent)
    {
        if (parent) parent->tree_lock.unlock();

        node->tree_lock.unlock();
        std::this_thread::yield();
        while (true)
        {
            node->tree_lock.lock();
            if (!node->valid)
            {
                node->tree_lock.unlock();
                return NULL;
            }

            auto child = getBalanceFactor(node) >= 2 ? node->left : node->right;
            if (child == NULL) return NULL;
            if (child->tree_lock.try_lock()) return child;
            node->tree_lock.unlock();
            std::this_thread::yield();
        }
    }

    void rotate(ConcurrentNode<T>* child, ConcurrentNode<T>* node, ConcurrentNode<T>* parent, bool left)
    {
        if (parent->left == node)
            parent->left = child;
        else
            parent->right = child;

        child->parent = parent;
        node->parent = child;
        auto grand_child = left ? child->left : child->right;
        if (left)
        {
            node->right = grand_child;
            if (grand_child != NULL)
                grand_child->parent = node;

            child->left = node;
            node->right_tree_height = child->left_tree_height;
            child->left_tree_height = std::max(node->left_tree_height, node->right_tree_height) + 1;
        }
        else
        {
            node->left = grand_child;
            if (grand_child != NULL)
                grand_child->parent = node;

            child->right = node;
            node->left_tree_height = child->right_tree_height;
            child->right_tree_height = std::max(node->left_tree_height, node->right_tree_height) + 1;
        }
    }

    void rebalance(ConcurrentNode<T> *node, ConcurrentNode<T> *child, bool is_left)
    {
        ConcurrentNode<T> *parent = NULL;

        if (node == _root)
        {
            unlockRebalance(node, child, NULL);
            return;
        }

        try
        {
            while (node != _root)
            {
                bool update_height = updateHeight(child, node, is_left);
                int bf = getBalanceFactor(node);

                if (!update_height && abs(bf) < 2)
                {
                    unlockRebalance(node, child, parent);
                    return;
                }

                while (bf >= 2 || bf <= -2)
                {
                    if ((is_left && bf <= -2) || (!is_left && bf >= 2))
                    {
                        if (child != NULL)
                            child->tree_lock.unlock();

                        child = is_left ? node->right : node->left;
                        if (!child->tree_lock.try_lock())
                        {
                            child = restart(node, parent);
                            if (!node->tree_lock.owns_lock())
                            {
                                unlockRebalance(node, child, parent);
                                return;
                            }

                            parent = NULL;
                            is_left = node->left == child;
                            bf = getBalanceFactor(node);
                            continue;
                        }

                        is_left = !is_left;
                    }

                    if ((is_left && getBalanceFactor(child) < 0) || (!is_left && getBalanceFactor(child) > 0))
                    {
                        // todo : test
                        ConcurrentNode<T> *grand_child = is_left ? child->right : child->left;
                        if (!grand_child->tree_lock.try_lock())
                        {
                            child->tree_lock.unlock();
                            child = restart(node, parent);
                            if (!node->tree_lock.owns_lock())
                            {
                                unlockRebalance(node, child, parent);
                                return;
                            }
                            parent = NULL;
                            is_left = node->left == child;
                            bf = getBalanceFactor(node);
                            continue;
                        }

                        rotate(grand_child, child, node, is_left);
                        child->tree_lock.unlock();
                        child = grand_child;
                    }

                    if (parent == NULL)
                        parent = lockParent(node);

                    rotate(child, node, parent, !is_left);
                    bf = getBalanceFactor(node);

                    if (bf >= 2 || bf <= -2)
                    {
                        parent->tree_lock.unlock();
                        parent = child;
                        child = NULL;
                        is_left = bf >= 2 ? false : true; // enforced to lock child
                        continue;
                    }

                    ConcurrentNode<T> *temp = child;
                    child = node;
                    node = temp;
                    is_left = node->left == child;
                    bf = getBalanceFactor(node);
                }

                if (child) child->tree_lock.unlock();

                child = node;
                node = (parent && parent->tree_lock.owns_lock()) ? parent : lockParent(node);
                is_left = node->left == child;
                parent = NULL;
            }
        }
        catch (const std::exception& e)
        {
            std::cout << "Error rebalance: " << e.what() << std::endl;
            unlockRebalance(node, child, parent);
        }

        unlockRebalance(node, child, parent);
    }

    void printRecursive(ConcurrentNode<T> *root) const
    {
        if (!root) return;

        printRecursive(root->left);
        std::cout << root->data << " ";
        printRecursive(root->right);
    }

    void deleteTree(ConcurrentNode<T> *root)
    {
        if (!root) return;

        deleteTree(root->left);
        deleteTree(root->right);

        delete root;
    }
};
