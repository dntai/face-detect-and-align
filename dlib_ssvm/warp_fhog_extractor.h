#ifndef WARP_FHOG_EXTRACTOR_H
#define WARP_FHOG_EXTRACTOR_H
#include <vector>
#include <dlib/geometry.h>
#include <dlib/array2d.h>
#include <dlib/image_transforms.h>
#include <dlib/image_processing/object_detector_abstract.h>
#include "opencv2/highgui/highgui.hpp"
#include "../chnfeature/Pyramid.h"
#include <dlib/opencv.h>
/* 
  * implement the interface defined in default_fhog_feature_extractor, so that we can use our own
  * fhog implementation in dlib's  <scan_fhog_pyramid>
  * 
  */
class warp_fhog_extractor
{
    /*!
    WHAT THIS OBJECT REPRESENTS
        The scan_fhog_pyramid object defined below is primarily meant to be used
        with the feature extraction technique implemented by extract_fhog_features().  
        This technique can generally be understood as taking an input image and
        outputting a multi-planed output image of floating point numbers that
        somehow describe the image contents.  Since there are many ways to define
        how this feature mapping is performed, the scan_fhog_pyramid allows you to
        replace the extract_fhog_features() method with a customized method of your
        choosing.  To do this you implement a class with the same interface as
        default_fhog_feature_extractor.       
        */
    public:
		
		
		inline dlib::point image_to_fhog (
        dlib::point p,
        int cell_size = 8,
        int filter_rows_padding = 1,
        int filter_cols_padding = 1
		) const
		{
			return p/cell_size  + dlib::point((filter_cols_padding-1)/2,(filter_rows_padding-1)/2);
		}




        dlib::rectangle image_to_feats (
            const dlib::rectangle& rect,       
            int cell_size,              
            int filter_rows_padding,    
            int filter_cols_padding     
            ) const
        {
            dlib::rectangle copy_rect = rect;
            dlib::point p1 = image_to_fhog(copy_rect.tl_corner(),cell_size,filter_rows_padding,filter_cols_padding);
            dlib::point p2 = image_to_fhog(copy_rect.br_corner(),cell_size,filter_rows_padding,filter_cols_padding);
			 return dlib::rectangle( p1, p2);
        }
            /*!
                requires
                    - cell_size > 0
                    - filter_rows_padding > 0,  equals one means no padding
                    - filter_cols_padding > 0
                ensures
                    - Maps a rectangle from the coordinates in an input image to the corresponding
                      area in the output feature image.
            !*/


        inline dlib::point fhog_to_image (
            dlib::point p,
            int cell_size = 8,
            int filter_rows_padding = 1,
            int filter_cols_padding = 1
        ) const
        {
            // Convert to image space and then set to the center of the cell.
            dlib::point offset;
            
            p = (p-dlib::point((filter_cols_padding-1)/2,(filter_rows_padding-1)/2))*cell_size ;
            if (p.x() >= 0 && p.y() >= 0) offset = dlib::point(cell_size/2,cell_size/2);
            if (p.x() <  0 && p.y() >= 0) offset = dlib::point(-cell_size/2,cell_size/2);
            if (p.x() >= 0 && p.y() <  0) offset = dlib::point(cell_size/2,-cell_size/2);
            if (p.x() <  0 && p.y() <  0) offset = dlib::point(-cell_size/2,-cell_size/2);
            return p + offset;
        }

        


        dlib::rectangle feats_to_image (
            const dlib::rectangle& rect,
            int cell_size,
            int filter_rows_padding,
            int filter_cols_padding
            ) const
        {
              return dlib::rectangle(fhog_to_image(rect.tl_corner(),cell_size,filter_rows_padding,filter_cols_padding),
                               fhog_to_image(rect.br_corner(),cell_size,filter_rows_padding,filter_cols_padding));

        }
            /*!
                requires
                    - cell_size > 0
                    - filter_rows_padding > 0
                    - filter_cols_padding > 0
                ensures
                    - Maps a rectangle from the coordinates of the hog feature image back to
                      the input image.
                    - Mapping from feature space to image space is an invertible
                      transformation.  That is, for any rectangle R we have:
                        R == image_to_feats(feats_to_image(R,cell_size,filter_rows_padding,filter_cols_padding),
                                                             cell_size,filter_rows_padding,filter_cols_padding).
            !*/

		 template <typename image_type>
            void operator()(
                const image_type& img,                      // in  : input image 
                dlib::array<dlib::array2d<float> >& hog,    // out : output fhog feature map
                int cell_size,                              // in : cell_size , eg 8 
                int filter_rows_padding,                    // in : padding ~~ eg 1( no padding )
                int filter_cols_padding                     // in :
            ) const
			{
				/*  fill the #hog with the fhog feature, use opencv's implementation */
				/*  since the toMat() function can only perform on the image_type( not const image_type), have to make a copy */

				/*  convert whatever image into array2d<unsigned char> */
				dlib::array2d<unsigned char> gray_imag;
				dlib::assign_image( gray_imag, img);
			    const cv::Mat mat_img  = dlib::toMat( gray_imag );
				if( mat_img.empty())
				{
					std::cout<<"Input image is empty, return "<<std::endl;
					return;
				}
				/*  again , chns contains the "pointer" to the computed_feature which is a big feature map */
				std::vector<cv::Mat> chns;
				cv::Mat computed_feature;
				if(!m_feature_generator.fhog( mat_img, computed_feature, chns, 0, cell_size, 9, 0.2))
				{
					std::cout<<"Fhog feature computation error , return "<<std::endl;
					return;
				}

				/*  copy the data to hog */
				const int num_planes = 31;
				hog.set_max_size(num_planes);		// make space for the 31 planes
				hog.set_size(num_planes);		// make space for the 31 planes

                int hog_row_offset = (filter_rows_padding-1)/2;
                int hog_col_offset = (filter_cols_padding-1)/2;

				for( unsigned int p_index=0;p_index<num_planes;p_index++)
				{
                    /* like dlib's implementation, adding extra border */
					hog[p_index].set_size( chns[p_index].rows+filter_rows_padding, chns[p_index].cols+filter_cols_padding);
                    dlib::assign_all_pixels( hog[p_index], 0);
					/*  both opencv and array2d hold image in row mahor format */
					for( unsigned int r_index=0;r_index<chns[p_index].rows;r_index++)
						for( unsigned int c_index=0;c_index<chns[p_index].cols;c_index++)
							hog[p_index][hog_row_offset+r_index][hog_col_offset+c_index] = chns[p_index].at<float>(r_index,c_index);
				}
                
                /*  check #hog */
                //std::cout<<"#hog size is "<<hog.size()<<std::endl;
                //std::cout<<"#hog max_size is "<<hog.max_size()<<std::endl;
                //for( unsigned long p_index=0;p_index<num_planes;p_index++)
                //{
				//	for( unsigned int r_index=0;r_index<chns[p_index].rows;r_index++)
                //    {
				//		for( unsigned int c_index=0;c_index<chns[p_index].cols;c_index++)
				//			std::cout<<hog[p_index][r_index][c_index]<<" ";
                //        std::cout<<std::endl;
                //    }
                //    std::cout<<"channle "<<p_index<<std::endl;
                //    int yu;
                //    std::cin>>yu;
                //}
			}
            /*!
                requires
                    - image_type == is an implementation of array2d/array2d_kernel_abstract.h
                    - img contains some kind of pixel type. 
                      (i.e. pixel_traits<typename image_type::type> is defined)
                ensures
                    - Extracts FHOG features by calling extract_fhog_features().  The results are
                      stored into #hog.  Note that if you are implementing your own feature extractor you can
                      pretty much do whatever you want in terms of feature extraction so long as the following
                      conditions are met:
                        - #hog.size() == get_num_planes()
                        - Each image plane in of #hog has the same dimensions.
                        - for all valid i, r, and c:
                            - #hog[i][r][c] == a feature value describing the image content centered at the 
                              following pixel location in img: 
                                feats_to_image(point(c,r),cell_size,filter_rows_padding,filter_cols_padding)
            !*/
            

            inline unsigned long get_num_planes (
            ) const { return 31; }
                   /*!
                       ensures
                           - returns the number of planes in the hog image output by the operator()
                             method.
                   !*/
	private:
			feature_Pyramids m_feature_generator;
            
};

inline void serialize   (const warp_fhog_extractor&, std::ostream&) {}
inline void deserialize (warp_fhog_extractor&, std::istream&) {}

#endif
