/*=========================================================================

  Program:   Insight Segmentation & Registration Toolkit
  Module:    $RCSfile: itkManifoldParzenWindowsListSampleFunction.txx,v $
  Language:  C++
  Date:      $Date: $
  Version:   $Revision: $

  Copyright (c) Insight Software Consortium. All rights reserved.
  See ITKCopyright.txt or http://www.itk.org/HTML/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef __itkManifoldParzenWindowsListSampleFunction_txx
#define __itkManifoldParzenWindowsListSampleFunction_txx

#include "itkManifoldParzenWindowsListSampleFunction.h"

namespace itk {
namespace Statistics {

template <class TListSample, class TOutput, class TCoordRep>
ManifoldParzenWindowsListSampleFunction<TListSample, TOutput, TCoordRep>
::ManifoldParzenWindowsListSampleFunction()
{
  this->m_KdTreeGenerator = NULL;

  this->m_EvaluationKNeighborhood = 50;
  this->m_RegularizationSigma = 1.0;

  this->m_CovarianceKNeighborhood = 0;
  this->m_KernelSigma = 0.0;
}

template <class TListSample, class TOutput, class TCoordRep>
ManifoldParzenWindowsListSampleFunction<TListSample, TOutput, TCoordRep>
::~ManifoldParzenWindowsListSampleFunction()
{
}

template <class TListSample, class TOutput, class TCoordRep>
void
ManifoldParzenWindowsListSampleFunction<TListSample, TOutput, TCoordRep>
::SetInputListSample( const InputListSampleType * ptr )
{
  this->m_ListSample = ptr;

  /**
   * Generate KdTree and create set of gaussians from input point set
   */
  this->m_KdTreeGenerator = TreeGeneratorType::New();
  this->m_KdTreeGenerator->SetSample( const_cast<InputListSampleType *>(
    this->m_ListSample.GetPointer() ) );
  this->m_KdTreeGenerator->SetBucketSize( 16 );
  this->m_KdTreeGenerator->Update();

  /**
   * Calculate covariance matrices
   */
  this->m_Gaussians.resize( this->GetInputListSample()->Size() );
  const unsigned int Dimension =
    this->GetInputListSample()->GetMeasurementVectorSize();

  unsigned long count = 0;
  typename InputListSampleType::ConstIterator It
    = this->GetInputListSample()->Begin();
  while( It != this->GetInputListSample()->End() )
    {
    InputMeasurementVectorType inputMeasurement = It.GetMeasurementVector();

    typename GaussianType::MeanType mean( Dimension );
    for( unsigned int d = 0; d < Dimension; d++ )
      {
      mean[d] = inputMeasurement[d];
      }

    this->m_Gaussians[count] = GaussianType::New();
    this->m_Gaussians[count]->SetMean( mean );

    if( this->m_CovarianceKNeighborhood > 0 )
      {
      /** Temporarily set the covariance */
      CovarianceMatrixType Cov( Dimension, Dimension );
      Cov.SetIdentity();
      Cov *= this->m_KernelSigma;
      this->m_Gaussians[count]->SetCovariance( Cov );

      Cov.Fill( 0 );

      typename TreeGeneratorType::KdTreeType
        ::InstanceIdentifierVectorType neighbors;
      unsigned int numberOfNeighbors = vnl_math_min(
           this->m_CovarianceKNeighborhood, static_cast<unsigned int>(
           this->GetInputListSample()->Size() ) );
      this->m_KdTreeGenerator->GetOutput()->Search(
        inputMeasurement, numberOfNeighbors, neighbors );

      RealType denominator = 0.0;
      for( unsigned int j = 0; j < numberOfNeighbors; j++ )
        {
        if( neighbors[j] != count
          && neighbors[j] < this->GetInputListSample()->Size() )
          {
          InputMeasurementVectorType neighbor
            = this->m_KdTreeGenerator->GetOutput()->GetMeasurementVector(
              neighbors[j] );

          RealType kernelValue = this->m_Gaussians[count]->Evaluate( neighbor );
          if( this->m_Weights.Size() == this->m_Gaussians.size() )
            {
            kernelValue *= this->m_Weights[count];
            }

          denominator += kernelValue;
          if( kernelValue > 0.0 )
            {
            for( unsigned int m = 0; m < Dimension; m++ )
              {
              for( unsigned int n = m; n < Dimension; n++ )
                {
                RealType covariance = kernelValue *
                  ( neighbor[m] - inputMeasurement[m] ) *
                    ( neighbor[n] - inputMeasurement[n] );
                Cov( m, n ) += covariance;
                Cov( n, m ) += covariance;
                }
              }
            }
          }
        }
      if( denominator > 0.0 )
        {
        Cov /= denominator;
        }
      for( unsigned int m = 0; m < Dimension; m++ )
        {
        Cov( m, m ) +=
          ( this->m_RegularizationSigma * this->m_RegularizationSigma );
        }
      this->m_Gaussians[count]->SetCovariance( Cov );
      }
    else
      {
      CovarianceMatrixType Cov( Dimension, Dimension );
      Cov.SetIdentity();
      Cov *= this->m_RegularizationSigma;
      this->m_Gaussians[count]->SetCovariance( Cov );
      }
    ++It;
    ++count;
    }

  /**
   * Calculate normalization factor
   */
  this->m_NormalizationFactor = 0.0;
  for( unsigned int i = 0; i < this->m_Gaussians.size(); i++ )
    {
    if( this->m_Weights.Size() == this->m_Gaussians.size() )
      {
      this->m_NormalizationFactor += this->m_Weights[i];
      }
    else
      {
      this->m_NormalizationFactor += 1.0;
      }
    }
}

template <class TListSample, class TOutput, class TCoordRep>
TOutput
ManifoldParzenWindowsListSampleFunction<TListSample, TOutput, TCoordRep>
::Evaluate( const InputMeasurementVectorType &measurement ) const
{
  unsigned int numberOfNeighbors = vnl_math_min(
    this->m_EvaluationKNeighborhood,
    static_cast<unsigned int>( this->m_Gaussians.size() ) );

  OutputType sum = 0.0;

  if( numberOfNeighbors == this->m_Gaussians.size() )
    {
    for( unsigned int j = 0; j < this->m_Gaussians.size(); j++ )
      {
      sum += static_cast<OutputType>(
        this->m_Gaussians[j]->Evaluate( measurement ) );
      }
    }
  else
    {
    typename TreeGeneratorType::KdTreeType
      ::InstanceIdentifierVectorType neighbors;
    this->m_KdTreeGenerator->GetOutput()->Search( measurement,
      numberOfNeighbors, neighbors );

    for( unsigned int j = 0; j < numberOfNeighbors; j++ )
      {
      sum += static_cast<OutputType>(
        this->m_Gaussians[neighbors[j]]->Evaluate( measurement ) );
      }
    }
  return static_cast<OutputType>( sum / this->m_NormalizationFactor );
}

/**
 * Standard "PrintSelf" method
 */
template <class TListSample, class TOutput, class TCoordRep>
void
ManifoldParzenWindowsListSampleFunction<TListSample, TOutput, TCoordRep>
::PrintSelf(
  std::ostream& os,
  Indent indent) const
{
  os << indent << "Regularization sigma: "
               << this->m_RegularizationSigma << std::endl;
  os << indent << "Evaluation K neighborhood: "
               << this->m_EvaluationKNeighborhood << std::endl;
  if( this->m_CovarianceKNeighborhood > 0 )
    {
    os << indent << "Covariance K neighborhood: "
                 << this->m_CovarianceKNeighborhood << std::endl;
    os << indent << "Kernel sigma: "
                 << this->m_KernelSigma << std::endl;
    }
}

} // end of namespace Statistics
} // end of namespace itk

#endif
