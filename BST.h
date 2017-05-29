
#include <iostream>

template<typename T>
struct BSTNode
{
    T data;
    int height;
    BSTNode<T> *left;
    BSTNode<T> *right;

    BSTNode(T data) :
        data(data),
        height(0),
        left(NULL),
        right(NULL)
    {
    }
};

template<typename T>
class Tree
{
public:
    virtual void insert(T data) = 0;
    virtual void remove(T data) = 0;
    virtual bool contains(T data) const = 0;
    virtual void print() const = 0;

protected:
    // Deleted nodes are replaced by smallest node in right subtree
    BSTNode<T>* findMin(BSTNode<T> *node)
    {
        while (node->left)
            node = node->left;
        return node;
    }

    void deleteTree(BSTNode<T> *root)
    {
        if (!root) return;

        deleteTree(root->left);
        deleteTree(root->right);

        delete root;
        root = NULL;
    }

    bool containsRecursive(BSTNode<T> *root, T data) const
    {
        if (!root) return false;
        if (root->data == data) return true;
        return (containsRecursive(root->left, data) || containsRecursive(root->right, data));
    }

    void printRecursive(BSTNode<T> *root) const
    {
        if (!root) return;

        printRecursive(root->left);
        std::cout << root->data << " ";
        printRecursive(root->right);
    }
};

template<typename T>
class BST : public Tree<T>
{
public:
    BST() :
        _root(NULL)
    {
    }

    ~BST()
    {
        Tree<T>::deleteTree(_root);
    }

    virtual void insert(T data)
    {
        if (!_root)
            _root = new BSTNode<T>(data);
        else
            insertRecursive(_root, data);
    }

    virtual void remove(T data)
    {
        _root = removeRecursive(_root, data);
    }
    
    virtual bool contains(T data) const
    {
        return Tree<T>::containsRecursive(_root, data);
    }

    virtual void print() const
    {
        Tree<T>::printRecursive(_root);
        std::cout << "\n";
    }

private:
    BSTNode<T> *_root;

    void insertRecursive(BSTNode<T> *root, T data)
    {
        // don't add value already in tree
        if (root->data == data)
            return;

        if (root->data < data)
        {
            if (!root->right)
                root->right = new BSTNode<T>(data);
            else
                insertRecursive(root->right, data);
        }
        else
        {
            if (!root->left)
                root->left = new BSTNode<T>(data);
            else
                insertRecursive(root->left, data);
        }
    }

    BSTNode<T>* removeRecursive(BSTNode<T> *root, T data)
    {
        if (!root) return root;

        if (root->data < data)
            root->right = removeRecursive(root->right, data);
        else if (root->data > data)
            root->left = removeRecursive(root->left, data);
        else
        {
        	if (!root->left && !root->right)
        	{
        	    delete root;
        	    root = NULL;
        	}
        	else if (!root->left)
        	{
        	    auto temp = root;
        	    root = root->right;
                delete temp;
        	}
        	else if (!root->right)
        	{
        	    auto temp = root;
        	    root = root->left;
                delete temp;
        	}
        	else
        	{
        	    // save children and delete node
        	    auto left = root->left;
        	    auto right = root->right;
        	    delete root;

        	    // remove and store min value from right subtree
        	    auto min = Tree<T>::findMin(right)->data;
        	    removeRecursive(right, min);

        	    // create replacement node centered around min value
        	    root = new BSTNode<T>(min);
        	    root->left = left;
        	    root->right = right;
        	}
        }

        return root;
    }
};

template<typename T>
class AVLTree : public Tree<T>
{
public:
    AVLTree() :
        _root(NULL)
    {
    }

    ~AVLTree()
    {
        Tree<T>::deleteTree(_root);
    }

    virtual void insert(T data)
    {
        _root = insertRecursive(_root, data);
    }

    virtual void remove(T data)
    {
        _root = removeRecursive(_root, data);
    }

    virtual bool contains(T data) const
    {
        return Tree<T>::containsRecursive(_root, data);
    }

    virtual void print() const
    {
        Tree<T>::printRecursive(_root);
        std::cout << "\n";
    }

private:
    BSTNode<T> *_root;

    BSTNode<T>* insertRecursive(BSTNode<T> *node, T data)
    {
        if (!node) return new BSTNode<T>(data);

        if (data < node->data)
            node->left = insertRecursive(node->left, data);
        else if (data > node->data)
            node->right = insertRecursive(node->right, data);
        else
            return node;

        return balance(node);
    }

    BSTNode<T>* balance(BSTNode<T> *node)
    {
        setHeight(node);
        if (balanceFactor(node) == 2)
        {
            if (balanceFactor(node->right) < 0)
                node->right = rotateRight(node->right);
            return rotateLeft(node);
        }

        if (balanceFactor(node) == -2)
        {
            int b = balanceFactor(node->left);
            if (b > 0)
                node->left = rotateLeft(node->left);
            return rotateRight(node);
        }

        return node;
    }

    BSTNode<T>* removeRecursive(BSTNode<T>* node, T data)
    {
        if (!node) return NULL;

        if (node->data < data)
            node->right = removeRecursive(node->right, data);
        else if (node->data > data)
            node->left = removeRecursive(node->left, data);
        else
        {
            auto left = node->left;
            auto right = node->right;
            delete node;

            if (!right) return left;

            auto min = Tree<T>::findMin(right);
            min->right = removeMin(right);
            min->left = left;
            return balance(min);
        }

        return balance(node);
    }

    BSTNode<T>* removeMin(BSTNode<T> *node)
    {
        if (!node->left)
            return node->right;
        node->left = removeMin(node->left);
        return balance(node);
    }

    int height(BSTNode<T> *node) const
    {
        return node ? node->height : 0;
    }

    int balanceFactor(BSTNode<T> *node) const
    {
        int hr = height(node->right);
        int hl = height(node->left);
        return hr - hl;
    }

    void setHeight(BSTNode<T> *node) const
    {
        auto left_height = height(node->left);
        auto right_height = height(node->right);
        node->height = (left_height > right_height ? left_height : right_height) + 1;
    }

    BSTNode<T>* rotateRight(BSTNode<T> *node)
    {
        auto pivet = node->left;
        node->left = pivet->right;
        pivet->right = node;
        setHeight(pivet);
        setHeight(node);
        return pivet;
    }

    BSTNode<T>* rotateLeft(BSTNode<T> *node)
    {
        auto pivet = node->right;
        node->right = pivet->left;
        pivet->left = node;
        setHeight(pivet);
        setHeight(node);
        return pivet;
    }
};
