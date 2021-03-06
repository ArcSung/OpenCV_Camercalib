#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/features2d/features2d.hpp>
#include <opencv2/calib3d.hpp>
#include "myCVClasses.hpp"
#include "Preprocess.h"
#include "LPD.h"


using namespace std;
using namespace cv;
using namespace std;

#define CAP_WIDTH  640
#define CAP_HEIGHT 480



Size boardSize( 7, 5 );
Size imageSize(CAP_WIDTH, CAP_HEIGHT);
const int CUBE_SIZE = 5;
const int CHESS_SIZE = 25;

vector<Point3f> contoursFindedObjectPoints;
vector<Point3f> _3DPoints;
vector< vector<Point2f> > contoursFinded;
vector<Point2f> corners;
myCV::CameraCalibration *camCalib;
Mat rvecs, tvecs;
cv::Mat cameraMatrix;
cv::Mat distCoeffs;
Point sPoint, tPoint;
int tlx, tly, brx, bry;
Point2f LL1, LL2, LL3, LL4;
Point2f L1, L2, L3, L4;

int nImages = 10;
bool done = false;



    class Cube{
        public :
            vector<Point3f> srcPoints3D;
            vector<Point2f> dstPoints2D;
        Cube(){
            //LOGI("cube init starts");
            float CUBExCHESS = CUBE_SIZE * CHESS_SIZE;
            for(int i=0;i<8;i++){
                switch(i){
                    case 0:
                        srcPoints3D.push_back(Point3f(0,0,0));
                    break;
                    case 1:
                        srcPoints3D.push_back(Point3f(CUBExCHESS,0,0));
                    break;
                    case 2:
                        srcPoints3D.push_back(Point3f(CUBExCHESS,CUBExCHESS,0));
                    break;
                    case 3:
                        srcPoints3D.push_back(Point3f(0,CUBExCHESS,0));
                    break;
                    case 4:
                        srcPoints3D.push_back(Point3f(0,0,CUBExCHESS));
                    break;
                    case 5:
                        srcPoints3D.push_back(Point3f(CUBExCHESS,0,CUBExCHESS));
                    break;
                    case 6:
                        srcPoints3D.push_back(Point3f(CUBExCHESS,CUBExCHESS,CUBExCHESS));
                    break;
                    case 7:
                        srcPoints3D.push_back(Point3f(0,CUBExCHESS,CUBExCHESS));
                    break;
                    default:
                    break;
            }
        }
        //LOGI("cube init ends");
    }
};

inline double dist(Point a, Point b){
        return sqrt( (a.x-b.x) * (a.x-b.x) + (a.y-b.y) * (a.y-b.y) );
    }

    void drawMarkerContours(Mat image, Mat mgray){
        //LOGI("drawMarkerContours starts");
        cv::Mat bin_img;
        cv::Mat imgGrayscaleScene;
        preprocess(image, imgGrayscaleScene, bin_img);
        //cv::threshold(mgray, bin_img, 0, 255, cv::THRESH_BINARY|cv::THRESH_OTSU);
        std::vector<std::vector<cv::Point> > contours;
        std::vector<cv::Vec4i> hierarchy;

        /*vector<Vec4i> lines;
        HoughLinesP(bin_img, lines, 1, CV_PI/180, 50, 50, 10 );
        for( size_t i = 0; i < lines.size(); i++ )
        {
           Vec4i l = lines[i];
           line( bin_img, Point(l[0], l[1]), Point(l[2], l[3]), Scalar(255,255,255), 3, LINE_AA);
        }*/
        //PlateDtect(image);
        //cv::imshow("image", bin_img);

        //return;

        cv::findContours(bin_img,
            contours,
            hierarchy,
            cv::RETR_TREE,
            cv::CHAIN_APPROX_SIMPLE);

        vector< vector< Point> >::iterator itc = contours.begin();
        for(;itc!=contours.end();){  //remove some contours size <= 100
            if(itc->size() <= 100)
                itc = contours.erase(itc);
            else
                ++itc;
        }


       // LOGI("approx poly");
        vector< vector< Point> > contours_poly( contours.size() );
        for( int i = 0; i < contours.size(); i++ )
        {
            approxPolyDP( Mat(contours[i]), contours_poly[i], 3, true ); // let contours more smooth
        }

        itc = contours_poly.begin();
        for(;itc!=contours_poly.end();){ // if contour has not four points, then we remove it
            if(itc->size() != 4)
                itc = contours_poly.erase(itc);
            else
                ++itc;
        }


        //LOGI("resize contours_poly");
        int csize = contours_poly.size();
        vector< Point > centers;
        vector< bool > isValid;
        for(int i=0;i<csize;i++){
            double x = 0, y = 0;
            for(int j=0;j<4;j++){
                x += contours_poly[i][j].x;
                y += contours_poly[i][j].y;
            }
            centers.push_back( Point( x/4, y/4 ) );
        }

        for(int i=0;i<csize;i++){
            bool ok = false;
            for(int j=0;j<csize;j++){
                if(i == j) continue;
                if(dist(centers[i], centers[j]) < 20){
                    ok = true;
                    break;
                }
            }
            if(ok) isValid.push_back(true);
            else isValid.push_back(false);
        }

        itc = contours_poly.begin();
        for(int i=0;itc!=contours_poly.end();++i){
            if(!isValid[i])
                itc = contours_poly.erase(itc);
            else
                ++itc;
        }

        contoursFinded.clear();

        for(int i=0;i<contours_poly.size();i++){
            vector< Point2f > temp;
            int minsum = 10000, maxsum = 0, mindiff = 10000, maxdiff = 10000;
            Point2f L1, L2, L3, L4;
            int tl1, br1;

            for(int j=0;j<contours_poly[i].size();j++){
                //temp.push_back( Point2f(contours_poly[i][j].x, contours_poly[i][j].y) );
                //circle(image, cvPoint(contours_poly[i][j].x, contours_poly[i][j].y), 3, CV_RGB(0, 0, 255), 3, CV_AA);
                if(contours_poly[i][j].x+ contours_poly[i][j].y < minsum)
                {
                    minsum = contours_poly[i][j].x+ contours_poly[i][j].y;
                    L1 = Point2f(contours_poly[i][j].x, contours_poly[i][j].y);
                    tl1 = j;
                }

                if(contours_poly[i][j].x+ contours_poly[i][j].y > maxsum)
                {
                    maxsum = contours_poly[i][j].x+ contours_poly[i][j].y;
                    L3 = Point2f(contours_poly[i][j].x, contours_poly[i][j].y);
                    br1 = j;
                }
            }

            for(int j=0;j<contours_poly[i].size();j++){

                if(j == br1 || j == tl1)
                    continue;
                if(contours_poly[i][j].x < mindiff)
                {
                    mindiff = contours_poly[i][j].x;
                    L4 = Point2f(contours_poly[i][j].x, contours_poly[i][j].y);
                }

                if(contours_poly[i][j].y < maxdiff)
                {
                    maxdiff = contours_poly[i][j].y;
                    L2 = Point2f(contours_poly[i][j].x, contours_poly[i][j].y);
                }
            }

            tlx = (L1.x < L4.x) ? L1.x : L4.x;
            tly = (L1.y < L2.y) ? L1.y : L2.y;
            brx = (L3.x > L2.x) ? L3.x : L2.x;
            bry = (L3.y > L4.y) ? L3.y : L4.y;

            LL1 = L1;
            LL2 = L2;
            LL3 = L3;
            LL4 = L4;

            temp.push_back(L1);
            temp.push_back(L2);
            temp.push_back(L3);
            temp.push_back(L4);
            circle(image, L1, 3, CV_RGB(0, 0, 255), 3, CV_AA);
            circle(image, L2, 3, CV_RGB(0, 0, 255), 3, CV_AA);
            circle(image, L3, 3, CV_RGB(0, 0, 255), 3, CV_AA);
            circle(image, L4, 3, CV_RGB(0, 0, 255), 3, CV_AA);
            contoursFinded.push_back(temp);
        }

        //LOGI("draw contour");
        /*cv::drawContours(image,
            contours_poly,
            -1,
            Scalar(128,128,0,255),
            2);*/

        bin_img.release();
        imgGrayscaleScene.release();
        //LOGI("drawMarkerContours ends");
    }


    void Init()
    {
        //Initialising the 3D-Points for the chessboard
        camCalib = new myCV::CameraCalibration(boardSize.width, boardSize.height, nImages);
        camCalib->Initialisation();

        int x2 = 100;
        float a = 0.2f;	

        contoursFindedObjectPoints.push_back(
                Point3f(0, 0, 0.0f));
        contoursFindedObjectPoints.push_back(
                Point3f(0, x2, 0.0f));
        contoursFindedObjectPoints.push_back(
                Point3f(x2, x2, 0.0f));
        contoursFindedObjectPoints.push_back(
                Point3f(x2, 0, 0.0f));

        float rot = 0.0f;
	    Point3f _3DPoint;
	    float y = (((5-1.0f)/2.0f)*a)+(a/2.0f);
	    float x = 0.0f;
	    for(int h = 0; h < 5; h++, y+=a){
		    x = (((7-2.0f)/2.0f)*(-a))-(a/2.0f);
		    for(int w = 0; w < 7; w++, x+=a){
			    _3DPoint.x = x;
			    _3DPoint.y = y;
			    _3DPoint.z = 0.0f;
			    _3DPoints.push_back(_3DPoint);
		    }
	    }

        FileStorage fs2("CameraCalib.xml", FileStorage::READ);

// first method: use (type) operator on FileNode.
        int frameCount = (int)fs2["frameCount"];

        std::string date;
// second method: use FileNode::operator >>
        fs2["calibrationDate"] >> date;

        fs2["cameraMatrix"] >> cameraMatrix;
        fs2["distCoeffs"] >> distCoeffs;
        fs2.release();

        if(frameCount == nImages)
        {
            done = true;
        }
        //PlateDtect_init();
    }

int main()
{
	    Init();

        //Mat& mGr  = *(Mat*)addrGray;
        //Mat& mRgb = *(Mat*)addrRgba;
        Mat mGr;
        Mat image;
        Mat dst_img;
        Mat bin_img;
        VideoCapture capture(0);
        capture >> image;
        Mat mRgb;


	while(1)									// Loop That Runs While done=FALSE
	{
		capture >> image;
		resize(image, mRgb , Size(640, 480), 0, 0, INTER_NEAREST);
		preprocess(mRgb, mGr, bin_img);
        if(!done) {
        	printf("camCalibing...\n");
            camCalib->GrabFrames(mGr, mRgb);

            done = camCalib->getInitialisation();

            if(done){
            	FileStorage fs("CameraCalib.xml", FileStorage::WRITE);

            	fs << "frameCount" << nImages;
            	time_t rawtime; time(&rawtime);
            	cameraMatrix = camCalib->getIntrinsicsMatrix();
            	distCoeffs = camCalib->getDistortionCoeffs();
            	fs << "calibrationDate" << asctime(localtime(&rawtime));
            	fs << "cameraMatrix" << cameraMatrix << "distCoeffs" << distCoeffs;
            	fs << "features" << "[";
            	for( int i = 0; i < 3; i++ )
            	{
                	int x = rand() % 640;
                	int y = rand() % 480;
                	uchar lbp = rand() % 256;

                	fs << "{:" << "x" << x << "y" << y << "lbp" << "[:";
                	for( int j = 0; j < 8; j++ )
                    	fs << ((lbp >> j) & 1);
                	fs << "]" << "}";
            	}
            	fs << "]";
            	fs.release();
            }	

        }
        else {
        	//printf("dectecting...\n");

            //drawMarkerContours(mRgb, mGr);
            bool  found = findChessboardCorners(mRgb, boardSize, corners, CV_CALIB_CB_ADAPTIVE_THRESH | CV_CALIB_CB_FILTER_QUADS);
            

            if(found) {
            	cornerSubPix(mGr, corners, Size(5, 5), Size(-1,-1), TermCriteria(CV_TERMCRIT_EPS+CV_TERMCRIT_ITER, 30, 0.1 ));
                if((corners[34].x + corners[34].y) < (corners[0].x + corners[0].y))
                {
                	corners = ReverserMat(corners);
                }	

                drawChessboardCorners(mRgb, boardSize, Mat(corners), found);

            	Mat m1(corners);
                Mat m2(_3DPoints);

                cv::solvePnP(m2,
                             m1,
                             cameraMatrix,
                             distCoeffs,
                             rvecs,
                             tvecs);

                Mat rmat;
                char str[200];
                Rodrigues(rvecs,rmat);
                double roll, pitch, yaw;

                roll = atan2(rmat.at<double>(1,0),rmat.at<double>(0,0));
                pitch = -asin(rmat.at<double>(2,0));
                yaw = atan2(rmat.at<double>(2,1),rmat.at<double>(2,2));

                roll = roll*180/3.1415;
                pitch = pitch*180/3.1415;
                yaw = yaw*180/3.1415;

                sprintf(str,"roll: %d ; pitch: %d ; yaw: %d", (int)(roll), (int)(pitch), (int)(yaw));

                printf("%s\n", str);

                putText(mRgb, str, Point(10,mRgb.rows - 40), FONT_HERSHEY_PLAIN, 2, CV_RGB(0,255,0));

                double alpha = 90 + (pitch - 30)/4 ; //the rotation around the x axis
                //double alpha = 90;   
                double beta = 90 - yaw;  //the rotation around the y axis
                //double beta = 90;
                double gamma = (roll > 90? 90 + (180 - roll) : 90 - roll); //the rotation around the z axis
                //double gamma = 90;
                sprintf(str,"alpha: %d ; beta: %d ; gamma: %d", (int)(alpha), (int)(beta), (int)(gamma));
                printf("%s\n", str);
               
                rotateImage(mRgb, dst_img, alpha, beta, gamma, 0, 0, 200, 200);


                m1.release();
                m2.release();
                rmat.release();
                //SrcROI.release();
                cv::imshow("dst", dst_img);
            }	
            else if(contoursFinded.size() > 0) {


                //Mat m1(corners);
                Mat m1(contoursFinded[0]);
                Mat m2(contoursFindedObjectPoints);

                /* perspective matrix*/
                /*int ROIW = brx - tlx - 1;
                int ROIH = bry - tly - 1;

                //Mat SrcROI = mRgb(Rect(tlx, tly, ROIW, ROIH));
                // 設定變換[之前]與[之後]的坐標 (左上,左下,右下,右上)
                cv::Point2f pts1[] = {LL1,LL4,LL3,LL2};
                cv::Point2f pts2[] = {cv::Point2f(0,0),cv::Point2f(0,ROIH),cv::Point2f(ROIW,ROIH),cv::Point2f(ROIW,0)};

                cv::Mat perspective_matrix = cv::getPerspectiveTransform(pts1, pts2);
                
                // 變換
                cv::warpPerspective(mRgb, dst_img, perspective_matrix, Size(ROIW, ROIH), cv::INTER_LINEAR);*/
                /* perspective matrix end*/

                //LOGI("src size: %d", contoursFinded[0].size());
                //LOGI("obj size: %d", contoursFindedObjectPoints.size());


                //LOGI("solvePnP");
                cv::solvePnP(m2,
                             m1,
                             cameraMatrix,
                             distCoeffs,
                             rvecs,
                             tvecs);

                //LOGI("projectPoints");
                //projectPoints(mCube.srcPoints3D, rvecs, tvecs, cameraMatrix, distCoeffs, mCube.dstPoints2D);

                /*LOGI("points::::::::::::");
                for(int i=0;i<8;i++){
                    LOGI("%f,%f,%f --> %f,%f",
                         mCube.srcPoints3D[i].x, mCube.srcPoints3D[i].y, mCube.srcPoints3D[i].z,
                         mCube.dstPoints2D[i].x, mCube.dstPoints2D[i].y);
                }*/

                /*sPoint = mCube.dstPoints2D[0];
                tPoint = mCube.dstPoints2D[1];
                cv::line(mRgb, sPoint, tPoint, Scalar(0,0,255,255),5,8,0);

                sPoint = mCube.dstPoints2D[0];
                tPoint = mCube.dstPoints2D[4];
                cv::line(mRgb, sPoint, tPoint, Scalar(255,0,0,255),5,8,0);

                sPoint = mCube.dstPoints2D[0];
                tPoint = mCube.dstPoints2D[3];
                cv::line(mRgb, sPoint, tPoint, Scalar(0,255,0,255),5,8,0);*/

                /*sPoint = Point((mCube.dstPoints2D[0].x+ mCube.dstPoints2D[2].x)/2, (mCube.dstPoints2D[0].y+ mCube.dstPoints2D[2].y)/2);
                tPoint = Point((mCube.dstPoints2D[0].x+ mCube.dstPoints2D[1].x)/2, (mCube.dstPoints2D[0].y+ mCube.dstPoints2D[1].y)/2);
                cv::line(mRgb, sPoint, tPoint, Scalar(255,0,0,255),5,8,0);

                tPoint = Point((mCube.dstPoints2D[0].x+ mCube.dstPoints2D[3].x)/2, (mCube.dstPoints2D[0].y+ mCube.dstPoints2D[3].y)/2);
                cv::line(mRgb, sPoint, tPoint, Scalar(0,255,0,255),5,8,0);

                tPoint = Point((mCube.dstPoints2D[4].x+ mCube.dstPoints2D[6].x)/2, (mCube.dstPoints2D[4].y+ mCube.dstPoints2D[6].y)/2);
                cv::line(mRgb, sPoint, tPoint, Scalar(0,0,255,255),5,8,0);*/

                //LOGI("calcue roll, pitch yaw");

                Mat rmat;
                char str[200];
                Rodrigues(rvecs,rmat);
                double roll, pitch, yaw;

                roll = atan2(rmat.at<double>(1,0),rmat.at<double>(0,0));
                pitch = -asin(rmat.at<double>(2,0));
                yaw = atan2(rmat.at<double>(2,1),rmat.at<double>(2,2));

                roll = roll*180/3.1415;
                pitch = pitch*180/3.1415;
                yaw = yaw*180/3.1415;

                sprintf(str,"roll: %d ; pitch: %d ; yaw: %d", (int)(roll), (int)(pitch), (int)(yaw));

                printf("%s\n", str);
                /*printf("\n");
                printf("perspective_matrix\n");
                printf("%f, %f, %f\n", perspective_matrix.at<double>(0,0), perspective_matrix.at<double>(0,1), perspective_matrix.at<double>(0,2));
                printf("%f, %f, %f\n", perspective_matrix.at<double>(1,0), perspective_matrix.at<double>(1,1), perspective_matrix.at<double>(1,2));
                printf("%f, %f, %f\n", perspective_matrix.at<double>(2,0), perspective_matrix.at<double>(2,1), perspective_matrix.at<double>(2,2));*/
                putText(mRgb, str,
                        Point(10,mRgb.rows - 40), FONT_HERSHEY_PLAIN, 2, CV_RGB(0,255,0));

                double alpha = 90 + (pitch)/4; //the rotation around the x axis
                double beta = 90 - (yaw > 0? 180 - yaw: 0 - 180 - yaw)/4;  //the rotation around the y axis
                //double gamma = 90 - (roll - 90); //the rotation around the z axis
                double gamma = 90;

                rotateImage(mRgb, dst_img, alpha, beta, gamma, 0, 0, 200, 200);

                /*if(ROIW < mRgb.rows && ROIH < mRgb.cols) {
                    //LOGI("ROI %d, %d", ROIW, ROIH);
                    resize(dst_img, dst_img , Size(150, 150), 0, 0, INTER_NEAREST);
                    //PlateDtect(dst_img);
                    dst_img.copyTo(mRgb(Rect(0, 0, 150, 150)));
                }*/
                //resize(dst_img, dst_img , Size(200, 200), 0, 0, INTER_NEAREST);

                m1.release();
                m2.release();
                rmat.release();
                //SrcROI.release();
                cv::imshow("dst", dst_img);

            }
            //LOGI("drawProcessing ends");


        }
        cv::imshow("result", mRgb);
        

        if(cv::waitKey(1) == 27){
			return 0;
		}
    }
}    

