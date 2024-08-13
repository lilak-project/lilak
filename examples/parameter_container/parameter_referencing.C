void parameter_referencing()
{
    auto par = new LKParameterContainer("user_parameters.mac");
    par -> Print();

    vector<int> colorArray = par -> GetParVInt("user/color_rainbow");
    for (auto i=0; i<colorArray.size(); ++i)
        cout << "color-" << i << " = " << colorArray[i] << endl;
    LKMisc::DrawColors(colorArray);
}
