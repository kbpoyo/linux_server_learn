
#include <iostream>
using namespace std;

struct node {
    int num;
    node* left;
    node* right;
};

node* get_tree(int mid_arr[], int mid_low, int mid_high, 
                int back_arr[], int back_low, int back_high) {

    if (mid_low > mid_high) {
        return NULL;
    }

    node *tree = new(node);
    tree->num = back_arr[back_high];
    

    int index = 0;
    for (int i = mid_low; i <= mid_high; i++) {
        if (mid_arr[i] == tree->num) {
            index = i;
            break;
        }
    }

    tree->left = get_tree(mid_arr, mid_low, index - 1, back_arr, back_low, back_low + (index - mid_low) - 1);
    tree->right = get_tree(mid_arr, index + 1, mid_high, back_arr, back_low + (index - mid_low), back_high - 1);

    return tree;        
}

bool flag_L[20]{};
void solve_L(node *tree, int level) {
    if (tree == NULL) return;

    if (!flag_L[level]) {
        cout << tree->num << ' ';
        flag_L[level] = true;
    }

    solve_L(tree->left, level + 1);
    solve_L(tree->right, level + 1);

}

bool flag_R[20]{};
void solve_R(node *tree, int level) {
    if (tree == NULL) return;

    if (!flag_R[level]) {
        cout << tree->num << ' ';
        flag_R[level] = true;
    }

    solve_R(tree->right, level + 1);
    solve_R(tree->left, level + 1);
}


void sort_scan(node *tree) {
    if (tree == NULL) return;
    sort_scan(tree->left);
    sort_scan(tree->right);
    cout << tree->num << " ";
}


int mid_arr[8] = {6, 8, 7, 4, 5, 1, 3, 2};
int back_arr[8] = {8, 5, 4, 7, 6, 3, 2, 1};


int main() {

    node* tree = get_tree(mid_arr, 0, sizeof(mid_arr) / sizeof(mid_arr[0]) - 1, back_arr, 0, sizeof(back_arr) / sizeof(back_arr[0]) - 1);
    solve_L(tree, 0);
    cout << endl;
    solve_R(tree, 0);


    return 0;

}