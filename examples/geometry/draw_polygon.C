void draw_polygon()
{
    LKDrawing draw;

    double half_size = 45;
    double r1 = 0.5*40;
    double r2 = 0.5*60;
    double r3 = 0.5*62;
    double r4 = 0.5*78;
    double dx = 45;
    double rc = 14/2.;
    double rt1 = 3.3/2.;
    double rt2 = 4.5/2.;
    double d0 = 17;
    double d1 = 20;

    auto hist = new TH2D("hist","", 100,-half_size,half_size, 100,-half_size,half_size);
    hist -> SetStats(0);
    draw.Add(hist);

    LKGeoPolygon pg1(0,0,r3,8,360./8/2); pg1.SetRMin(r3);
    LKGeoPolygon pg2(0,0,r4,8,360./8/2); pg2.SetRMin(r4);
    draw.Add(pg1.GetGraph(),"samel");
    draw.Add(pg2.GetGraph(),"samel");

    draw.Add(LKGeo2DBox(0,0,2*r3,2*r3,0).GetGraph(),"samel");
    draw.Add(LKGeo2DBox(0,0,2*r4,2*r4,0).GetGraph(),"samel");

    draw.Add(LKGeo2DBox(0,0,d1,d1,0).GetGraph(),"samel");

    draw.Add(LKGeoCircle(0,0,r1).GetGraph(),"samel");
    draw.Add(LKGeoCircle(0,0,r2).GetGraph(),"samel");

    draw.Add(LKGeoCircle(0,0,rc).GetGraph(),"samel");
    for (auto rt : {rt1,rt2}) {
        draw.Add(LKGeoCircle(+d0,+d0,rt).GetGraph(),"samel");
        draw.Add(LKGeoCircle(+d0,-d0,rt).GetGraph(),"samel");
        draw.Add(LKGeoCircle(-d0,-d0,rt).GetGraph(),"samel");
        draw.Add(LKGeoCircle(-d0,+d0,rt).GetGraph(),"samel");
    }
    draw.Add(LKGeoCircle(-d0,+0.5*(r3+r4),rt1).GetGraph(),"samel");
    draw.Add(LKGeoCircle(+d0,+0.5*(r3+r4),rt1).GetGraph(),"samel");
    draw.Add(LKGeoCircle(-d0,-0.5*(r3+r4),rt1).GetGraph(),"samel");
    draw.Add(LKGeoCircle(+d0,-0.5*(r3+r4),rt1).GetGraph(),"samel");

    auto graph = LKGeo2DBox(0,0,dx,2*r4,0).GetGraph();
    draw.Add(graph,"samel");

    draw.SetCanvasSize(600,600);
    draw.Draw();
}
