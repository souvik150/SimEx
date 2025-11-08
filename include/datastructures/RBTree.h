#pragma once
#include <memory>
#include <functional>
#include <iostream>

/**
 * @brief Generic header-only Red-Black Tree.
 *        Supports both ascending and descending order via comparator.
 *        Designed for order books (price → PriceLevel).
 */

template <typename Key, typename Value, typename Compare = std::less<Key>>
class RBTree {
private:
    enum class Color { RED, BLACK };

    struct Node {
        Key key;
        Value value;
        Color color;
        Node* parent;
        Node* left;
        Node* right;

        Node(const Key& k, Value&& v, Color c, Node* p)
            : key(k), value(std::move(v)), color(c), parent(p), left(nullptr), right(nullptr) {}
    };

    Node* root_ = nullptr;
    size_t size_ = 0;
    Compare comp_{};

public:
    RBTree() = default;
    ~RBTree() { clear(root_); }

    RBTree(const RBTree&) = delete;
    RBTree& operator=(const RBTree&) = delete;

    // ==========================================================
    //  PUBLIC API
    // ==========================================================

    bool insert(const Key& key, Value&& val) {
        Node* parent = nullptr;
        Node* cur = root_;

        while (cur) {
            parent = cur;
            if (comp_(key, cur->key))
                cur = cur->left;
            else if (comp_(cur->key, key))
                cur = cur->right;
            else {
                cur->value = std::move(val);
                return false;
            }
        }

        Node* node = new Node(key, std::move(val), Color::RED, parent);
        if (!parent)
            root_ = node;
        else if (comp_(key, parent->key))
            parent->left = node;
        else
            parent->right = node;

        fixInsert(node);
        size_++;
        return true;
    }

    Value* find(const Key& key) const {
        Node* cur = root_;
        while (cur) {
            if (comp_(key, cur->key))
                cur = cur->left;
            else if (comp_(cur->key, key))
                cur = cur->right;
            else
                return &cur->value;
        }
        return nullptr;
    }

    bool erase(const Key& key) {
        Node* node = findNode(key);
        if (!node) return false;
        deleteNode(node);
        size_--;
        return true;
    }

    Value* findMin() const {
        Node* n = minNode(root_);
        return n ? &n->value : nullptr;
    }

    Value* findMax() const {
        Node* n = maxNode(root_);
        return n ? &n->value : nullptr;
    }

    bool empty() const { return root_ == nullptr; }
    size_t size() const { return size_; }

    void printInOrder() const { printInOrder(root_); }

    /**
     * @brief Returns pointer to best (top-of-book) value.
     * For ascending tree (std::less) → lowest key (min).
     * For descending tree (std::greater) → highest key (min in descending order).
     */
    Value* best() const {
        // In descending trees, "minNode" = highest price.
        return findMin();
    }

    /**
     * @brief Returns pointer to worst (tail) value.
     * For ascending → highest; for descending → lowest.
     */
    Value* worst() const {
        return findMax();
    }

private:
    // ==========================================================
    //  INTERNAL HELPERS
    // ==========================================================
    Node* findNode(const Key& key) const {
        Node* cur = root_;
        while (cur) {
            if (comp_(key, cur->key))
                cur = cur->left;
            else if (comp_(cur->key, key))
                cur = cur->right;
            else
                return cur;
        }
        return nullptr;
    }

    Node* minNode(Node* n) const {
        if (!n) return nullptr;
        while (n->left) n = n->left;
        return n;
    }

    Node* maxNode(Node* n) const {
        if (!n) return nullptr;
        while (n->right) n = n->right;
        return n;
    }

    void clear(Node* n) {
        if (!n) return;
        clear(n->left);
        clear(n->right);
        delete n;
    }

    void printInOrder(Node* n) const {
        if (!n) return;
        printInOrder(n->left);
        std::cout << n->key << " ";
        printInOrder(n->right);
    }

    // ==========================================================
    //  ROTATION / BALANCING LOGIC
    // ==========================================================
    void rotateLeft(Node* x) {
        Node* y = x->right;
        x->right = y->left;
        if (y->left) y->left->parent = x;
        y->parent = x->parent;
        if (!x->parent)
            root_ = y;
        else if (x == x->parent->left)
            x->parent->left = y;
        else
            x->parent->right = y;
        y->left = x;
        x->parent = y;
    }

    void rotateRight(Node* y) {
        Node* x = y->left;
        y->left = x->right;
        if (x->right) x->right->parent = y;
        x->parent = y->parent;
        if (!y->parent)
            root_ = x;
        else if (y == y->parent->right)
            y->parent->right = x;
        else
            y->parent->left = x;
        x->right = y;
        y->parent = x;
    }

    void fixInsert(Node* node) {
        while (node != root_ && node->parent->color == Color::RED) {
            Node* parent = node->parent;
            Node* grand = parent->parent;

            if (parent == grand->left) {
                Node* uncle = grand->right;
                if (uncle && uncle->color == Color::RED) {
                    parent->color = Color::BLACK;
                    uncle->color = Color::BLACK;
                    grand->color = Color::RED;
                    node = grand;
                } else {
                    if (node == parent->right) {
                        node = parent;
                        rotateLeft(node);
                        parent = node->parent;
                    }
                    parent->color = Color::BLACK;
                    grand->color = Color::RED;
                    rotateRight(grand);
                }
            } else {
                Node* uncle = grand->left;
                if (uncle && uncle->color == Color::RED) {
                    parent->color = Color::BLACK;
                    uncle->color = Color::BLACK;
                    grand->color = Color::RED;
                    node = grand;
                } else {
                    if (node == parent->left) {
                        node = parent;
                        rotateRight(node);
                        parent = node->parent;
                    }
                    parent->color = Color::BLACK;
                    grand->color = Color::RED;
                    rotateLeft(grand);
                }
            }
        }
        root_->color = Color::BLACK;
    }

    void transplant(Node* u, Node* v) {
        if (!u->parent)
            root_ = v;
        else if (u == u->parent->left)
            u->parent->left = v;
        else
            u->parent->right = v;
        if (v)
            v->parent = u->parent;
    }

    void deleteNode(Node* z) {
        Node* y = z;
        Node* x = nullptr;
        Color yColor = y->color;

        if (!z->left) {
            x = z->right;
            transplant(z, z->right);
        } else if (!z->right) {
            x = z->left;
            transplant(z, z->left);
        } else {
            y = minNode(z->right);
            yColor = y->color;
            x = y->right;
            if (y->parent == z) {
                if (x) x->parent = y;
            } else {
                transplant(y, y->right);
                y->right = z->right;
                if (y->right) y->right->parent = y;
            }
            transplant(z, y);
            y->left = z->left;
            if (y->left) y->left->parent = y;
            y->color = z->color;
        }

        delete z;
        if (yColor == Color::BLACK)
            fixDelete(x);
    }

    void fixDelete(Node* x) {
        while (x != root_ && (!x || x->color == Color::BLACK)) {
            Node* parent = x ? x->parent : nullptr;
            if (!parent) break;
            bool leftChild = (x == parent->left);
            Node* sibling = leftChild ? parent->right : parent->left;

            if (sibling && sibling->color == Color::RED) {
                sibling->color = Color::BLACK;
                parent->color = Color::RED;
                if (leftChild)
                    rotateLeft(parent);
                else
                    rotateRight(parent);
                sibling = leftChild ? parent->right : parent->left;
            }

            if ((!sibling->left || sibling->left->color == Color::BLACK) &&
                (!sibling->right || sibling->right->color == Color::BLACK)) {
                if (sibling) sibling->color = Color::RED;
                x = parent;
            } else {
                if (leftChild && sibling->right && sibling->right->color == Color::BLACK) {
                    sibling->left->color = Color::BLACK;
                    sibling->color = Color::RED;
                    rotateRight(sibling);
                    sibling = parent->right;
                } else if (!leftChild && sibling->left && sibling->left->color == Color::BLACK) {
                    sibling->right->color = Color::BLACK;
                    sibling->color = Color::RED;
                    rotateLeft(sibling);
                    sibling = parent->left;
                }

                if (sibling)
                    sibling->color = parent->color;
                parent->color = Color::BLACK;
                if (leftChild && sibling && sibling->right)
                    sibling->right->color = Color::BLACK;
                else if (!leftChild && sibling && sibling->left)
                    sibling->left->color = Color::BLACK;

                if (leftChild)
                    rotateLeft(parent);
                else
                    rotateRight(parent);
                break;
            }
        }

        if (x) x->color = Color::BLACK;
    }
};
