#ifndef RECTPACKER_H
#define RECTPACKER_H

#include <string.h>

class RectPacker
{
public:
    RectPacker();
    RectPacker(int w, int h);
    ~RectPacker();

    struct Rect
    {
        Rect() : x(0), y(0), right(0), bottom(0) { }
        Rect(int rx, int ry, int rr, int rb) : x(rx), y(ry), right(rr), bottom(rb) { }

        inline int width() const { return right - x + 1; }
        inline int height() const { return bottom - y + 1; }

        void clear() { x = y = right = bottom = 0; }

        int x, y;
        int right, bottom;
    };

    bool isValid() const { return root != nullptr; }

    struct Node
    {
        Node(Node* parent = nullptr)
            : offsetX(0), offsetY(0), leaf(false)
        {
            nodes[0] = nodes[1] = nullptr;
            nodes[2] = parent;
        }
        ~Node() {
            if (nodes[2]) {
                if (nodes[2]->nodes[0] == this)
                    nodes[2]->nodes[0] = nullptr;
                if (nodes[2]->nodes[1] == this)
                    nodes[2]->nodes[1] = nullptr;
            }
            delete nodes[0];
            delete nodes[1];
        }

        inline Node* parent() { return nodes[2]; }

        Node* nodes[3];
        Rect rect;
        int offsetX, offsetY;

        bool leaf;
    };

    unsigned int serial() const { return ser; }

    void init(int w, int h);
    void resize(int w, int h);

    Node* insert(int w, int h);
    void destroy();

    void take(RectPacker& other);

    unsigned int size() const { return root ? (root->rect.width() * root->rect.height()) : 0; }
    unsigned int used() const;

    static void clear(Node* node);

private:
    static void resizeNode(Node* node, int nr, int nb, int dw, int dh);

    Node* insertSize(Node* node, int w, int h);

    Node* root;
    unsigned int ser;
};

#endif
