static vector<float,n> operator*<let n:int> (vector<float,n> v, matrix<float,n,n> mat)
{
	return mul(v,mat);
}

static vector<float,n> operator*<let n:int> ( matrix<float,n,n> mat,vector<float,n> v)
{
	return mul(mat,v);
}

