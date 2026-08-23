#pragma once

namespace std
{
	buffer BuiltIn
	{
	    //reserved parameters,autoupdated by app:
		float t;//time counter
		float dt;//time passed from last frame
		float2 extent;//render area extent in pixels
	};
};

static vector<float,n> operator*<let n:int> (vector<float,n> v, matrix<float,n,n> mat)
{
	return mul(v,mat);
}

static vector<float,n> operator*<let n:int> ( matrix<float,n,n> mat,vector<float,n> v)
{
	return mul(mat,v);
}





