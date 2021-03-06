/*
 * main.cpp
 *
 *  Created on: Aug 17, 2013
 *      Author: zhonghua
 */
#include "main.h"

#include "Global.h"
#include "GrabCut.h"

#include <stdlib.h>

Image<Color> *displayImage;
GrabCut* gc = NULL;

// Some state variables for the UI
Real xstart, ystart, xend, yend;
bool box = false;
bool initialized = false;
bool is_left = false;
bool is_right = false;
bool refining = false;
bool showMask = false;
int displayType = 0;
int edits = 0;


// used to evaluation
Mat img_ground_truth;

void init()
{
	//set the background color to black (RGBA)
	glClearColor(0.0,0.0,0.0,0.0);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
}

// Draw the image and paint the chosen mask over the top.
void display()
{
	glClear(GL_COLOR_BUFFER_BIT);

	gc->display(displayType);

	if (showMask)
	{
		gc->overlayAlpha();
	}

	if (box)
	{
		glColor4f( 1, 1, 1, 1 );
		glBegin( GL_LINE_LOOP );
		glVertex2d( xstart, ystart );
		glVertex2d( xstart, yend );
		glVertex2d( xend, yend );
		glVertex2d( xend, ystart );
		glEnd();
	}

	glFlush();
	glutSwapBuffers();
}

void idle()
{
	int changed = 0;

	if (refining)
		{
		changed = gc->refineOnce();
		glutPostRedisplay();
		}

	if (!changed)
		{
		refining = false;
		glutIdleFunc(NULL);
		}
}

void mouse(int button, int state, int x, int y)
{
	y = displayImage->height() - y;

	switch(button)
	{
	case GLUT_LEFT_BUTTON:
		if (state==GLUT_DOWN)
		{
			is_left = true;

			if (!initialized)
			{
				xstart = x; ystart = y;
				box = true;
			}
		}

		if( state==GLUT_UP )
		{
			is_left = false;

			if( initialized )
			{
				gc->refineOnce();
				glutPostRedisplay();
			}

			else
			{
				xend = x; yend = y;
				gc->initialize(xstart, ystart, xend, yend);
				gc->fitGMMs();
				box = false;
				initialized = true;
				showMask = true;
				glutPostRedisplay();
			}
		}
		break;

	case GLUT_RIGHT_BUTTON:
		if( state==GLUT_DOWN )
		{
			is_right = true;
		}
		if( state==GLUT_UP )
		{
			is_right = false;

			if( initialized )
			{
				gc->refineOnce();
				glutPostRedisplay();
			}
		}
		break;

	default:
		break;
	}
}

void motion(int x, int y)
{
	y = displayImage->height() - y;

	if( box == true )
	{
		xend = x; yend = y;
		glutPostRedisplay();
	}

	if( initialized )
	{
		if( is_left )
			gc->setTrimap(x-2,y-2,x+2,y+2,TrimapForeground);

		if( is_right )
			gc->setTrimap(x-2,y-2,x+2,y+2,TrimapBackground);

		glutPostRedisplay();
	}
}

void evaluation(){
	if(!img_ground_truth.data) {
		cout<<"ground truth not found"<<endl;
		return;
	}

	Mat seg;
	gc->getSegmentationResult(&seg);

	cout<<"seg result got "<<seg.rows<<"x"<<seg.cols<<endl;

	Mat im;
	cv::resize(seg, im, Size(), (float)img_ground_truth.rows / seg.rows, (float)img_ground_truth.cols/seg.cols);
	cout<<"image enlarged to "<<im.rows<<"x"<<im.cols<<endl;

	double score = Matting::evaluate(img_ground_truth, im);

	Mat ground_truth_to_display;
	cv::resize(img_ground_truth, ground_truth_to_display, Size(), (float)seg.rows/img_ground_truth.rows, (float)seg.cols/img_ground_truth.cols);
	vector<Mat> ch;
	split(ground_truth_to_display, ch);

	cout<<"score = "<<score<<endl;

	cv::namedWindow("seg result", CV_WINDOW_AUTOSIZE );
	cv::imshow("seg result", seg);
	cv::namedWindow("ground truth", CV_WINDOW_AUTOSIZE );
	cv::imshow("ground truth", ch[3]);
	//
	cv::waitKey(2);
}

void keyboard(unsigned char key, int x, int y)
{
	y = displayImage->height() - y;

	switch (key)
	{
	case 'q':case 'Q':				// quit
		exit(0);
		break;

	case ' ':						// space bar show/hide alpha mask
		showMask = !showMask;
		break;

	case '1': case 'i': case 'I':	// choose the image
		displayType = 0;
		break;

	case '2': case 'g': case 'G':	// choose GMM index mask
		displayType = 1;
		break;

	case '3': case 'n': case 'N':	// choose N-Link mask
		displayType = 2;
		break;

	case '4': case 't': case 'T':	// choose T-Link mask
		displayType = 3;
		break;

	case 'r':						// run GrabCut refinement
		refining = true;
		glutIdleFunc(idle);
		break;

	case 'o':						// run one step of GrabCut refinement
		gc->refineOnce();
		glutPostRedisplay();
		break;

	case 'l':
		gc->fitGMMs();			// rerun the Orchard-Bowman GMM clustering
		glutPostRedisplay();
		break;

	case 'e':					// evaluation
		evaluation();
		break;

	case 27:
		refining = false;
		glutIdleFunc(NULL);

	default:
		break;
	}

	glutPostRedisplay();
}

bool parse_arg(int argc, char** argv){
	if(argc < 2) {
		return false;
	}
	int i=1;
	while(i<argc){
		string arg = argv[i];
		if(arg == "-m"){
			if(i+1 < argc){
				g_setting.matting_filename = argv[++i];
				g_setting.matting_mode = true;
			}
			else{
				return false;
			}
		}
		else if(arg == "-t"){
			if(i+1 < argc){
				g_setting.training_filename = argv[++i];
				g_setting.training_mode = true;
			}else{
				return false;
			}
		}
		else if(arg == "-ta"){
			if(i+1 < argc) {
				g_setting.training_dir = argv[++i];
				g_setting.training_batch_mode = true;
			}
		}
		else if(arg == "-e") {
			g_setting.enable_evaluation = true;
		}
		else
		{
			g_setting.matting_batch_mode=true;
			g_setting.input_dir = arg;
			g_setting.enable_gui = false;
		}
		++i;
	}
	return true;
}

void print_usage(int argc, char** argv){
	cout<<"usage: "<<argv[0]<<" [-e] [-m filename] [-t filename] [-ta input_dir] [input_dir]"<<endl;
	cout<<"\t-e enable evaluation, will NOT save result to file"<<endl;
	cout<<"\t-m filename: mat single image"<<endl;
	cout<<"\t-t filename: train single image"<<endl;
	cout<<"\t-ta input_dir: train entire directory"<<endl;
	cout<<"\tinput_dir: mat entire directory"<<endl;
}

string get_profile_name(const string& input){
	return input.substr(0, input.find_last_of('.')) + "-profile.jpg";
}

vector<string> get_files(const string& input_dir){
	vector<string> files;

	DIR *dpdf;
	struct dirent *epdf;

	dpdf = opendir(input_dir.c_str());
	if (dpdf != NULL){
	   while (epdf = readdir(dpdf)){
		   string filename = string(epdf->d_name);
		   if(filename.find(".jpg")!=filename.length() - 4)
			   continue;
		   if(filename.find("-profile") == std::string::npos)
			   files.push_back(filename);
	   }
	}
	return files;
}

bool matting(const string& input, const string& output, Mat* min, Mat* mout, const string* ground_truth = NULL, double* score = NULL)
{
	Timer t;
	Matting M;

	double read_file_cost = 0, matting_cost = 0, write_file_cost = 0, evaulation_cost = 0;

	t.restart();

	*min = imread(input, CV_LOAD_IMAGE_COLOR);   // Read the file

	read_file_cost = t.getElapsedMilliseconds();

	if(! min->data )                              // Check for invalid input
	{
		cout << " ! Error ! Could not open or find the image" << std::endl ;
		return false;
	}

	// =========================================
	// matting
	// =========================================

	t.restart();

	M.mat(*min, *mout);

	matting_cost = t.getElapsedMilliseconds();

	// =========================================
	// save result if no evaluation switch specified
	// =========================================
	if(!g_setting.enable_evaluation){
		t.restart();

		imwrite(output, *mout);

		write_file_cost = t.getElapsedMilliseconds();
	}

	cout<<"[Matting] from "<<input<<" to "<<output
		<<" size = "<<min->rows<<"x"<<min->cols
		<<" read = "<<read_file_cost<<"ms"<<" mat =  "<<matting_cost<<"ms write = "<<write_file_cost<<"ms"<<endl;

	if(g_setting.enable_evaluation && ground_truth){

		// ==========================================
		// evaluation
		// ==========================================

		t.restart();

		img_ground_truth = imread(*ground_truth, CV_LOAD_IMAGE_COLOR);

		read_file_cost = t.getElapsedMilliseconds();

		if(!img_ground_truth.data){
			cout << " ! Error ! Could not open or find the ground truth image " << *ground_truth<< std::endl ;
			return false;
		}

		double p = Matting::evaluate(img_ground_truth, *mout);

		if(score) *score = p;

		evaulation_cost = t.getElapsedMilliseconds() - read_file_cost;

		cout<<"[Evaluation] with "<<*ground_truth<<" score = "<<*score<<endl;
	}

	return true;
}

bool training(const string& input, const string& profile)
{
	Timer t;
	Mat img_org, img_profile;
	double read_file_cost, training_cost;
	t.restart();

	img_org = imread(input, CV_LOAD_IMAGE_COLOR);
	if(!img_org.data)
	{
		cout << " ! Error ! Could not open or find the image" <<input<< std::endl ;
		return false;
	}

	img_profile = imread(profile, CV_LOAD_IMAGE_COLOR);
	if(!img_profile.data)
	{
		cout<< " ! Error ! Could not open or find the image "<<profile<< std::endl ;
		return false;
	}

	read_file_cost = t.getElapsedMilliseconds();

	t.restart();
	Matting M;
	M.train(img_org, img_profile);
	training_cost = t.getElapsedMilliseconds();

	cout<<"[Training] from "<<input<<" & "<<profile
		<<" size = "<<img_org.rows<<"x"<<img_org.cols
		<<" read = "<<read_file_cost<<"ms"<<" train =  "<<training_cost<<"ms"<<endl;

	return true;
}

void run_batch(const string& input_dir)
{
	vector<string> files = get_files(input_dir);

	Mat min, mout;
	double total_score = 0.0;


	for(vector<string>::const_iterator it = files.begin(); it != files.end(); ++ it)
	{
		const string filename = input_dir + "/" + *it;
		const string profile_filename = get_profile_name(filename);
		string output_filename = get_profile_name(*it);
		double score = 0.0;

		matting(filename, output_filename, &min, &mout, &profile_filename, &score);
		total_score += score;
	}

	if(g_setting.enable_evaluation)
		cout<<"Average score = "<< total_score / files.size()<<endl;
}

void train_batch(const string& input_dir)
{
	vector<string> files = get_files(input_dir);

	for(vector<string>::const_iterator it = files.begin(); it != files.end(); ++ it)
	{
		const string& filename = input_dir + "/" + *it;
		string training_filename = get_profile_name(filename);

		training(filename, training_filename);
	}

	Matting::dump_training_results();
}

int main(int argc, char** argv){

	if(!parse_arg(argc, argv)){
		print_usage(argc, argv);
		return 0;
	}

	if(g_setting.matting_batch_mode)
	{
		run_batch(g_setting.input_dir);
	}

	if(g_setting.training_batch_mode)
	{
		train_batch(g_setting.training_dir);
	}

	if(g_setting.training_mode)
	{
		training(g_setting.training_filename, get_profile_name(g_setting.training_filename));
		Matting::dump_training_results();
	}

	if(g_setting.matting_mode)
	{
		Mat min, mout;
		double score;

		string profile_name = get_profile_name(g_setting.matting_filename);

		Image<Color>* image = loadForOCV( g_setting.matting_filename );

		img_ground_truth = imread(profile_name, -1);

		cout<<"ground truth:"
			<<" dim = "<<img_ground_truth.dims
			<<" channels = "<<img_ground_truth.channels()
			<<" size = "<<img_ground_truth.size()<<endl;

		if (image)
		{
			displayImage = image;

			gc = new GrabCut( image );

			glutInit(&argc,argv);
			glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);

			glutInitWindowSize(displayImage->width(),displayImage->height());
			glutInitWindowPosition(100,100);

			glutCreateWindow("wesee - mt");

			glOrtho(0,displayImage->width(),0,displayImage->height(),-1,1);

			init();

			glutDisplayFunc(display);
			glutMouseFunc(mouse);
			glutMotionFunc(motion);
			glutKeyboardFunc(keyboard);

			glutMainLoop();		//note: this will NEVER return.
		}

	}

	return 0;
}
