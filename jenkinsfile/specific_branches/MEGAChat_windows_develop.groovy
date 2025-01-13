pipeline {
    agent { label 'windows && amd64' }
    options {
        buildDiscarder(logRotator(numToKeepStr: '135', daysToKeepStr: '21'))
        gitLabConnection('GitLabConnectionJenkins')
        ansiColor('xterm')
    }
    environment {
        _MSPDBSRV_ENDPOINT_ = "${BUILD_TAG}"
        APIURL_TO_TEST = "https://g.api.mega.co.nz/"
        SDK_BRANCH = "develop"
        MEGACHAT_BRANCH = "develop"
        RUN_TESTS = false
        BUILD_OPTIONS = ' '
        BUILD_TYPE = "Release" 
        BUILD_DIR = "build_dir"
    }
    stages {
        stage('Checkout SDK and MEGAchat'){
            steps {
                checkout([
                    $class: 'GitSCM', 
                    branches: [[name: "origin/${env.MEGACHAT_BRANCH}"]],
                    userRemoteConfigs: [[ url: "${env.GIT_URL_MEGACHAT}", credentialsId: "12492eb8-0278-4402-98f0-4412abfb65c1" ]],
                ])
                dir('third-party\\mega'){
                    sh "echo Cloning SDK branch \"${SDK_BRANCH}\""
                    checkout([
                        $class: 'GitSCM',
                        branches: [[name: "origin/${SDK_BRANCH}"]],
                        userRemoteConfigs: [[ url: "${env.GIT_URL_SDK}", credentialsId: "12492eb8-0278-4402-98f0-4412abfb65c1" ]],
                    ])
                }
                script{
                    megachat_sources_workspace = WORKSPACE
                }
            }
        }
        stage('Build MEGAchat'){
            environment{
                VCPKGPATH  = "${megachat_sources_workspace}\\..\\..\\vcpkg"
                TMP       = "${megachat_sources_workspace}\\tmp"
                TEMP      = "${megachat_sources_workspace}\\tmp"
                TMPDIR    = "${megachat_sources_workspace}\\tmp"
            }
            steps{
                dir(megachat_sources_workspace){
                    sh "cmake -DCMAKE_BUILD_TYPE=${BUILD_TYPE} -DVCPKG_ROOT='${VCPKGPATH}' ${BUILD_OPTIONS} -DCMAKE_VERBOSE_MAKEFILE=ON \
                        -DENABLE_CHATLIB_QTAPP=OFF -DENABLE_CHATLIB_TESTS=ON -DUSE_FFMPEG=OFF -DUSE_FREEIMAGE=OFF \
                        -S '${megachat_sources_workspace}' -B '${megachat_sources_workspace}'\\\\${BUILD_DIR}\\\\"
                    sh "cmake --build '${megachat_sources_workspace}'\\\\${BUILD_DIR} --config ${BUILD_TYPE} -j2"
                }
            }
        }
    }
    post {
        always {
            deleteDir()
        }
    }
}
// vim: syntax=groovy tabstop=4 shiftwidth=4
