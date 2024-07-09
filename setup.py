from setuptools import setup
from setuptools.dist import Distribution


# Force a platform-specific wheel to be generated
# This is required as our extension is built outside the Python build system
class BinaryDistribution(Distribution):
    def has_ext_modules(*args, **kwargs):
        return True


setup(distclass=BinaryDistribution)
