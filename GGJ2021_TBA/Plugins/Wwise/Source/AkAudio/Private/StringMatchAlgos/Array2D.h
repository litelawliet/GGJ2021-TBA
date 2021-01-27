// Copyright (c) 2006-2019 Audiokinetic Inc. / All Rights Reserved

/*=============================================================================
	Array2D.h:
=============================================================================*/

#pragma once

template <class T>
class Array2D : public TArray<T>
{
public:
	typedef int32 size_type;
	typedef T reference;

	Array2D(size_type in_sizeX, size_type in_sizeY)
		: TArray<T>()
		, m_sizeX(in_sizeX)
		, m_sizeY(in_sizeY)
	{
		TArray<T>::Init(0, in_sizeX * in_sizeY);
	}
	Array2D(size_type in_sizeX, size_type in_sizeY, T in_defaultValue)
		: TArray<T>()
		, m_sizeX(in_sizeX)
		, m_sizeY(in_sizeY)
	{
		TArray<T>::Init(in_defaultValue, in_sizeX * in_sizeY);
	}

	inline reference& operator() (size_type in_x, size_type in_y)
	{
		return At(in_x, in_y);
	}

	inline const reference& operator() (size_type in_x, size_type in_y) const
	{
		return At(in_x, in_y);
	}

	inline reference& At(size_type in_x, size_type in_y)
	{
		return TArray<T>::GetData()[in_y*m_sizeX + in_x];
	}

	inline const reference& At(size_type in_x, size_type in_y) const
	{
		return TArray<T>::GetData()[in_y*m_sizeX + in_x];
	}

	size_type m_sizeX;
	size_type m_sizeY;
};
