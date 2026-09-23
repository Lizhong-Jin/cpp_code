#pragma once

#include <iostream>
#include <unordered_set>
#include <algorithm>
#include <set>
#include <queue>

using namespace std;

enum class Color {RED, BLACK};
struct Node {
    int val;
    Color color;
    Node* left;
    Node* right;
    Node* parent;
    Node(): val(0), color(Color::BLACK), left(nullptr), right(nullptr), parent(nullptr) {}
    Node(int _val): val(_val), color(Color::BLACK), left(nullptr), right(nullptr), parent(nullptr) {}
    Node(int _val, Color _color): val(_val), color(_color), left(nullptr), right(nullptr), parent(nullptr) {}
    Node(int _val, Color _color, Node* _left, Node* _right, Node* _parent):
        val(_val), color(_color), left(_left), right(_right), parent(_parent) {}
};

class RedBlackTree {
private:
    Node* root;
    Node* nil;
    int n; // 红黑树大小

    // 递归删除所有节点
    void clear(Node* node) {
        if (node == nil) return;
        clear(node->left);
        Node* r=node->right;
        delete node;
        clear(r);
    }
    // 红黑树左旋
    void left_rotate(Node* node) {
        Node* parent = node->parent;
        Node* rightson = node->right;
        // 更新节点node
        node->right = rightson->left;
        node->parent = rightson;
        // 更新节点rightson
        if (rightson->left != nil) {
            rightson->left->parent = node;
        }
        rightson->left = node;
        rightson->parent = parent;
        // 更新节点parent
        if (parent == nil) { // 若node为根节点
            root =rightson;  // 更新根节点
        }else if (node == parent->left) {
            parent->left = rightson;
        }else {
            parent->right = rightson;
        }
    }
    // 红黑树右旋
    void right_rotate(Node* node) {
        Node* parent = node->parent;
        Node* leftson = node->left;
        // 更新节点node
        node->left = leftson->right;
        node->parent = leftson;
        // 更新节点leftson
        if (leftson->right != nil) {
            leftson->right->parent = node;
        }
        leftson->right = node;
        leftson->parent = parent;
        // 更新节点parent
        if (parent == nil) { // 若node为根节点
            root = leftson;  // 更新根节点
        }else if (node == parent->left) {
            parent->left = leftson;
        }else {
            parent->right = leftson;
        }
    }
    // 中序遍历
    void inorder_traversal(Node* node, vector<int>& res) {
        if (node == nil) return;
        inorder_traversal(node->left, res);
        res.emplace_back(node->val);
        inorder_traversal(node->right, res);
    }
public:
    RedBlackTree() {
        nil = new Node();
        root = nil;
        n = 0;
    }
    ~RedBlackTree() {
        clear(root);
        delete nil;
    }
    // 插入元素
    void insert(int val) {
        if (root == nil) {
            root = new Node(val, Color::BLACK, nil, nil, nil);
            n=1;
            return;
        }
        Node* node = root;
        Node* prev = nil;
        while (node != nil) {
            if (val < node->val) {
                prev = node;
                node = node->left;
            }else if (val > node->val) {
                prev = node;
                node = node->right;
            }else return; // 如果插入值在红黑树中已经存在，则无需插入
        }
        Node* new_node = new Node(val, Color::RED, nil, nil, prev);
        n++; // 红黑树大小增加1
        if (prev->val > val) prev->left = new_node;
        else prev->right = new_node;
        // 如果插入节点的父节点为黑色，无须调整
        if (prev->color == Color::BLACK) return;
        // 否则需要调整
        // 递归调整至父节点为黑色或为根节点为止
        while (new_node->parent->color == Color::RED && new_node->parent != root) {
            // 若new_node->parent为其父节点的左子节点
            if (new_node->parent->parent->left == new_node->parent) {
                Node* uncle= new_node->parent->parent->right;
                // 若叔叔节点为红色，调整颜色即可
                if (uncle->color == Color::RED) {
                    uncle->color = Color::BLACK;
                    new_node->parent->color = Color::BLACK;
                    new_node->parent->parent->color = Color::RED;
                    // 递归调整new_node的祖父节点
                    new_node= new_node->parent->parent;
                }else {
                    // 若new_node为其父节点的右节点，需旋转至同向
                    if (new_node->parent->right == new_node) {
                        new_node=new_node->parent;
                        left_rotate(new_node);
                    }
                    new_node->parent->color = Color::BLACK;
                    new_node->parent->parent->color = Color::RED;
                    right_rotate(new_node->parent->parent);
                }
            }
            // 若new_node->parent为其父节点的右子节点
            else {
                Node* uncle= new_node->parent->parent->left;
                // 若叔叔节点为红色，调整颜色即可
                if (uncle->color == Color::RED) {
                    uncle->color = Color::BLACK;
                    new_node->parent->color = Color::BLACK;
                    new_node->parent->parent->color = Color::RED;
                    // 递归调整new_node的祖父节点
                    new_node= new_node->parent->parent;
                }else {
                    // 若new_node为其父节点的左子节点，需旋转至同向
                    if (new_node->parent->left == new_node) {
                        new_node=new_node->parent;
                        right_rotate(new_node);
                    }
                    new_node->parent->color = Color::BLACK;
                    new_node->parent->parent->color = Color::RED;
                    left_rotate(new_node->parent->parent);
                }
            }
        }
        root->color = Color::BLACK;
    }
    // 删除元素
    void erase(int val) {
        if (n==0) return;
        // 若只有一个节点，直接删除
        if (n==1) {
            n--;
            delete root;
            root = nil;
            return;
        }
        Node* node = root; // 待删除节点
        while (node != nil && node->val != val) {
            if (val < node->val) {
                node = node->left;
            }else {
                node = node->right;
            }
        }
        // 如果没有找到该节点，无需删除
        if (node == nil) return;

        // 用v节点替换u节点
        auto replace=[&](Node* u, Node* v) {
            if (u==root) {
                root=v;
            }else if (u->parent->left == u) {
                u->parent->left = v;
            }else {
                u->parent->right = v;
            }
            v->parent = u->parent;
        };
        // 查找节点u的后继节点(node右子树的最小节点，即最左节点)
        auto search=[&](Node* u)->Node* {
            u=u->right;
            while (u->left != nil) {
                u = u->left;
            }
            return u;
        };

        n--; // 红黑树大小减1
        // 若待删除节点node有两个不为nil的子节点，将其和后继节点交换
        if (node->left != nil && node->right != nil) {
            Node* succeed_node=search(node);
            swap(node->val,succeed_node->val);
            node=succeed_node;
        }
        // 若待删除节点node只有一个非nil的子节点, 此时node必定为黑色，子节点必为红色
        // 直接删除，用子节点将其替换即可
        if (node->left != nil && node->right == nil) {
            Node* sub_node=node->left;
            sub_node->color = Color::BLACK;
            replace(node,sub_node);
            delete node;
        }
        else if (node->left == nil && node->right != nil) {
            Node* sub_node=node->right;
            sub_node->color = Color::BLACK;
            replace(node,sub_node);
            delete node;
        }
        // 若待删除节点node的子节点都为nil, 即node为叶子节点
        else {
            // 若node为红色，直接删除即可
            if (node->color == Color::RED) {
                if (node->parent->left == node) {
                    node->parent->left = nil;
                }else {
                    node->parent->right = nil;
                }
                delete node;
            }
            // 若node为黑色
            else {
                // 将node标记为双黑
                Node* double_black=node;
                // 调整红黑树，消去双黑标记
                while (double_black != root && double_black->color == Color::BLACK) {
                    Node* parent=double_black->parent;
                    if (parent->left == double_black) {
                        Node* bro=parent->right;
                        // 若兄弟节点为红色，通过调整将兄弟节点变为黑色
                        if (bro->color == Color::RED) {
                            bro->color = Color::BLACK;
                            parent->color = Color::RED;
                            left_rotate(parent);
                            bro=parent->right;
                        }

                        // 若两个子节点都为黑色
                        if (bro->left->color == Color::BLACK && bro->right->color == Color::BLACK) {
                            // 若父节点为红色，交换父节点和兄弟节点颜色即可完成调整
                            if (parent->color == Color::RED) {
                                parent->color = Color::BLACK;
                                bro->color = Color::RED;
                                double_black = root;
                            }
                            // 若父节点为黑色，将父节点标记为双黑，继续调整
                            else {
                                bro->color = Color::RED;
                                double_black = parent;
                            }
                        }else{
                            // 若远侄节点为黑色, 旋转调整至远侄节点为红色的情况
                            if (bro->right->color == Color::BLACK) {
                                bro->color = Color::RED;
                                bro->left->color = Color::BLACK;
                                right_rotate(bro);
                                bro=parent->right;
                            }
                            // 现在远侄节点为红色，旋转即可调整完毕
                            bro->color = parent->color;
                            parent->color = Color::BLACK;
                            bro->right->color = Color::BLACK;
                            left_rotate(parent);
                            double_black = root;
                        }
                    }else {
                        Node* bro=parent->left;
                        // 若兄弟节点为红色，通过调整将兄弟节点变为黑色
                        if (bro->color == Color::RED) {
                            bro->color = Color::BLACK;
                            parent->color = Color::RED;
                            right_rotate(parent);
                            bro=parent->left;
                        }

                        // 若两个子节点都为黑色
                        if (bro->left->color == Color::BLACK && bro->right->color == Color::BLACK) {
                            // 若父节点为红色，交换父节点和兄弟节点颜色即可完成调整
                            if (parent->color == Color::RED) {
                                parent->color = Color::BLACK;
                                bro->color = Color::RED;
                                double_black = root;
                            }
                            // 若父节点为黑色，将父节点标记为双黑，继续调整
                            else {
                                bro->color = Color::RED;
                                double_black = parent;
                            }
                        }else{
                            // 若远侄节点为黑色, 旋转调整至远侄节点为红色的情况
                            if (bro->left->color == Color::BLACK) {
                                bro->color = Color::RED;
                                bro->right->color = Color::BLACK;
                                left_rotate(bro);
                                bro=parent->left;
                            }
                            // 现在远侄节点为红色，旋转即可调整完毕
                            bro->color = parent->color;
                            parent->color = Color::BLACK;
                            bro->left->color = Color::BLACK;
                            right_rotate(parent);
                            double_black = root;
                        }
                    }
                }
                if (node->parent->left == node) {
                    node->parent->left = nil;
                }else {
                    node->parent->right = nil;
                }
                delete node;
                root->color = Color::BLACK;
            }
        }
    }
    void clear() { // 清空红黑树
        clear(root);
        n=0;
    }
    int size() { // 获取红黑树大小
        return n;
    }
    bool empty() { // 检查是否为空
        return n==0;
    }
    bool find(int val) { // 检查元素是否在红黑树中
        Node* node = root;
        while (node != nil && node->val != val) {
            if (node->val < val) {
                node = node->right;
            }else {
                node = node->left;
            }
        }
        return node != nil;
    }
    Node* root_node() { // 返回红黑树根节点
        if (root==nil) return nullptr;
        return root;
    }
    vector<int> inorder() { // 返回红黑树中序遍历
        if (n==0) return {};
        vector<int> res;
        inorder_traversal(root, res);
        return res;
    }
    void print() { // 打印红黑树
        if (n==0) return;
        queue<pair<int, Node*>> q;
        q.emplace(-1, root);
        while (!q.empty()) {
            int m=q.size();
            while (m>0) {
                auto [val, node] = q.front();
                q.pop();
                if (node->left!=nil) {
                    q.emplace(node->val, node->left);
                }
                if (node->right!=nil) {
                    q.emplace(node->val, node->right);
                }
                if (node->color==Color::RED) cout <<val<<"->"<<node->val<<"-red"<<"  ";
                if (node->color==Color::BLACK) cout <<val<<"->"<<node->val<<"-black"<<"  ";
                m--;
            }
            cout << endl;
        }
    }
};

void RDTree_test() {}