#include "RectPacker.h"
#include <assert.h>

#include <memory>
#include <vector>

// #define PACK_DEBUG
#ifdef PACK_DEBUG
#include <stdio.h>
#endif

RectPacker::RectPacker()
    : root(nullptr), ser(0)
{
}

RectPacker::RectPacker(int w, int h)
    : root(nullptr), ser(0)
{
    init(w, h);
}

RectPacker::~RectPacker()
{
    delete root;
}

static void used_helper(RectPacker::Node* node, unsigned int* used)
{
    if (node->leaf) {
        *used += (node->rect.width() * node->rect.height());
        return;
    }
    if (node->nodes[0])
        used_helper(node->nodes[0], used);
    if (node->nodes[1])
        used_helper(node->nodes[1], used);
}

unsigned int RectPacker::used() const
{
    if (!root)
        return 0;
    unsigned int u = 0;
    used_helper(root, &u);
    return u;
}

void RectPacker::init(int w, int h)
{
    ++ser;
    delete root;
    root = new Node;
    root->rect.right = w - 1;
    root->rect.bottom = h - 1;
}

void RectPacker::resize(int w, int h)
{
    const int diffw = root->rect.right - (w - 1);
    const int diffh = root->rect.bottom - (h - 1);
    assert(diffw > 0 && diffh > 0);
    resizeNode(root, w - 1, h - 1, diffw, diffh);
}

void RectPacker::resizeNode(Node* node, int nr, int nb, int dw, int dh)
{
    if (node->rect.right > nr)
        node->rect.right -= dw;
    assert(node->rect.right > 0);
    if (node->rect.bottom > nb)
        node->rect.bottom -= dh;
    assert(node->rect.bottom > 0);

    if (node->nodes[0])
        resizeNode(node->nodes[0], nr, nb, dw, dh);
    if (node->nodes[1])
        resizeNode(node->nodes[1], nr, nb, dw, dh);
}

void RectPacker::destroy()
{
    delete root;
    root = nullptr;
}

void RectPacker::clear(Node* node)
{
    delete node->nodes[0];
    node->nodes[0] = nullptr;
    delete node->nodes[1];
    node->nodes[1] = nullptr;
    node->leaf = false;
}

void RectPacker::take(RectPacker& other)
{
    ++ser;
    delete root;
    root = other.root;
    other.root = nullptr;
}

struct NodeState
{
    NodeState()
        : mode(Start), node(nullptr)
    {
    }
    NodeState(RectPacker::Node* n)
        : mode(Start), node(n)
    {
    }

    enum { Start, Left, Right } mode;
    RectPacker::Node* node;
};

RectPacker::Node* RectPacker::insertSize(Node* node, int w, int h)
{
    std::vector<NodeState> nodes;
    nodes.reserve(1024);
    nodes.push_back(node);
    while (!nodes.empty()) {
        NodeState& state = nodes.back();
        Node* cur = state.node;
#ifdef PACK_DEBUG
        printf("walking down %d,%d+%dx%d (mode 0x%x)\n",
               cur->rect.x, cur->rect.y,
               cur->rect.width(), cur->rect.height(),
               state.mode);
#endif
        if (cur->nodes[0]) {
            switch (state.mode) {
            case NodeState::Start:
                state.mode = NodeState::Left;
                nodes.push_back(cur->nodes[0]);
                break;
            case NodeState::Left:
                state.mode = NodeState::Right;
                nodes.push_back(cur->nodes[1]);
                break;
            case NodeState::Right:
                nodes.pop_back();
                break;
            }
            continue;
        }
        if (cur->leaf) {
            nodes.pop_back();
            continue;
        }
        if (cur->rect.width() < w || cur->rect.height() < h) {
            nodes.pop_back();
            continue;
        }
        if (cur->rect.width() == w && cur->rect.height() == h) {
            cur->leaf = true;
            return cur;
        }
        cur->nodes[0] = new Node(cur);
        cur->nodes[1] = new Node(cur);

        int dw = cur->rect.width() - w;
        int dh = cur->rect.height() - h;
        assert(dw >= 0 && dh >= 0);
        if (dw > dh) {
#ifdef PACK_DEBUG
            printf("left\n");
#endif
            cur->nodes[0]->rect = Rect(cur->rect.x, cur->rect.y,
                                        cur->rect.x + w - 1, cur->rect.bottom);
            cur->nodes[1]->rect = Rect(cur->rect.x + w, cur->rect.y,
                                        cur->rect.right, cur->rect.bottom);
        } else {
#ifdef PACK_DEBUG
            printf("right\n");
#endif
            cur->nodes[0]->rect = Rect(cur->rect.x, cur->rect.y,
                                        cur->rect.right, cur->rect.y + h - 1);
            cur->nodes[1]->rect = Rect(cur->rect.x, cur->rect.y + h,
                                        cur->rect.right, cur->rect.bottom);
        }
#ifdef PACK_DEBUG
        printf("created nodes %d,%d+%dx%d, %d,%d+%dx%d (wanting %dx%d)\n",
               cur->nodes[0]->rect.x, cur->nodes[0]->rect.y,
               cur->nodes[0]->rect.width(), cur->nodes[0]->rect.height(),
               cur->nodes[1]->rect.x, cur->nodes[1]->rect.y,
               cur->nodes[1]->rect.width(), cur->nodes[1]->rect.height(),
               w, h);
#endif
        assert(state.mode == NodeState::Start);
    }
    return nullptr;
}

RectPacker::Node* RectPacker::insert(int w, int h)
{
    return insertSize(root, w, h);
}
