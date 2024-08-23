#include "make_group.C"

LKDrawingCluster* make_cluster(bool draw=true)
{
    auto cluster = new LKDrawingCluster();
    cluster -> Add(make_group(false,0));
    cluster -> Add(make_group(false,1));
    cluster -> Add(make_group(false,2));
    cluster -> Add(make_group(false,3));
    cluster -> Add(make_group(false,4));

    if (draw) cluster -> Draw();

    return cluster;
}
