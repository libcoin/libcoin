#include <coin/Point.h>

using namespace std;

Point::Point(int curve) : _ec_point(NULL), _ec_group(EC_GROUP_new_by_curve_name(curve)) {
    _ec_point = EC_POINT_new(_ec_group);
}

Point::Point(const Point& point) : _ec_group(EC_GROUP_new_by_curve_name(NID_secp256k1)) {
    EC_GROUP_copy(_ec_group, point._ec_group);
    _ec_point = EC_POINT_new(_ec_group);
    EC_POINT_copy(_ec_point, point._ec_point);
}

Point::Point(const EC_POINT* p, int curve) : _ec_group(EC_GROUP_new_by_curve_name(curve)) {
    _ec_point = EC_POINT_new(_ec_group);
    EC_POINT_copy(_ec_point, p);
}

Point::Point(Infinity inf, int curve) : _ec_group(EC_GROUP_new_by_curve_name(curve)) {
    _ec_point = EC_POINT_new(_ec_group);
    EC_POINT_set_to_infinity(_ec_group, _ec_point);
}

Point::Point(const CBigNum& x, const CBigNum& y, int curve) : _ec_group(EC_GROUP_new_by_curve_name(curve)) {
    _ec_point = EC_POINT_new(_ec_group);
    if(!EC_POINT_set_affine_coordinates_GFp(_ec_group, _ec_point, &x, &y, NULL))
        throw runtime_error("Trying to set ec point, but it might not be on curve!");
}

Point::~Point() {
    EC_POINT_clear_free(_ec_point);
    EC_GROUP_clear_free(_ec_group);
}

Point& Point::operator=(const Point& point) {
    _ec_group = EC_GROUP_new_by_curve_name(NID_secp256k1);
    EC_GROUP_copy(_ec_group, point._ec_group);
    _ec_point = EC_POINT_new(_ec_group);
    EC_POINT_copy(_ec_point, point._ec_point);
    
    return *this;
}

CBigNum Point::X() const {
    CBigNum x;
    CBigNum y;
    EC_POINT_get_affine_coordinates_GFp(_ec_group, _ec_point, &x, &y, NULL);
    return x;
}

CBigNum Point::Y() const {
    CBigNum x;
    CBigNum y;
    EC_POINT_get_affine_coordinates_GFp(_ec_group, _ec_point, &x, &y, NULL);
    return y;
}

Point& Point::operator+=(const Point& point) {
    EC_POINT* r = EC_POINT_new(_ec_group);
    EC_POINT_add(_ec_group, r, _ec_point, point._ec_point, NULL);
    EC_POINT_clear_free(_ec_point);
    _ec_point = r;
    return *this;
}

EC_GROUP* Point::ec_group() const {
    return _ec_group;
}

EC_POINT* Point::ec_point() const {
    return _ec_point;
}

Point operator*(const CBigNum& m, const Point& Q) {
    if (!EC_POINT_is_on_curve(Q.ec_group(), Q.ec_point(), NULL))
        throw std::runtime_error("Q not on curve");
    
    Point R(Q);
    
    if (!EC_POINT_mul(R.ec_group(), R.ec_point(), NULL, Q.ec_point(), &m, NULL))
        throw std::runtime_error("Multiplication error");
    
    return R;
}

bool operator==(const Point& P, const Point& Q) {
    return (EC_POINT_cmp(P.ec_group(), P.ec_point(), Q.ec_point(), NULL) == 0);
}

