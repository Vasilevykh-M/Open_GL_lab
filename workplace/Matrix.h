#pragma once
class Matrix
{
	float m_matrix[16];
public:
	static Matrix trans(float x, float y, float z);
	static Matrix RotX(float a);
	static Matrix RotY(float a);
	static Matrix RotZ(float a);

	Matrix operator *(const Matrix& mat);
};

