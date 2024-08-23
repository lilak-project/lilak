#include "make_cluster.C"

void data_viewer()
{
    auto viewer = new LKDataViewer(make_cluster(false));
    viewer -> Draw();
}
